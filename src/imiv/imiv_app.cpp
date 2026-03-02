// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"
#include "imiv_file_dialog.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <imgui.h>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
#    include <imgui_impl_glfw.h>
#    include <imgui_impl_vulkan.h>
#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>
#endif

#if defined(IMGUI_ENABLE_TEST_ENGINE)
#    include <imgui_te_context.h>
#    include <imgui_te_engine.h>
#    include <imgui_te_exporters.h>
#    include <imgui_te_ui.h>
#endif

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_image_window_title = "Image";

    enum class UploadDataType : uint32_t {
        UInt8  = 0,
        UInt16 = 1,
        UInt32 = 2,
        Half   = 3,
        Float  = 4,
        Double = 5,
        Unknown = 255
    };

    size_t upload_data_type_size(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return 1;
        case UploadDataType::UInt16: return 2;
        case UploadDataType::UInt32: return 4;
        case UploadDataType::Half: return 2;
        case UploadDataType::Float: return 4;
        case UploadDataType::Double: return 8;
        default: break;
        }
        return 0;
    }

    const char* upload_data_type_name(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return "u8";
        case UploadDataType::UInt16: return "u16";
        case UploadDataType::UInt32: return "u32";
        case UploadDataType::Half: return "half";
        case UploadDataType::Float: return "float";
        case UploadDataType::Double: return "double";
        default: break;
        }
        return "unknown";
    }

    bool map_spec_type_to_upload(TypeDesc spec_type, UploadDataType& upload_type,
                                 TypeDesc& read_format)
    {
        if (spec_type == TypeUInt8) {
            upload_type = UploadDataType::UInt8;
            read_format = TypeUInt8;
            return true;
        }
        if (spec_type == TypeUInt16) {
            upload_type = UploadDataType::UInt16;
            read_format = TypeUInt16;
            return true;
        }
        if (spec_type == TypeUInt32) {
            upload_type = UploadDataType::UInt32;
            read_format = TypeUInt32;
            return true;
        }
        if (spec_type == TypeHalf) {
            upload_type = UploadDataType::Half;
            read_format = TypeHalf;
            return true;
        }
        if (spec_type == TypeFloat) {
            upload_type = UploadDataType::Float;
            read_format = TypeFloat;
            return true;
        }
        if (spec_type == TypeDesc::DOUBLE) {
            upload_type = UploadDataType::Double;
            read_format = TypeDesc::DOUBLE;
            return true;
        }
        return false;
    }

    struct LoadedImage {
        std::string path;
        int width              = 0;
        int height             = 0;
        int nchannels          = 0;
        UploadDataType type    = UploadDataType::Unknown;
        size_t channel_bytes   = 0;
        size_t row_pitch_bytes = 0;
        std::vector<unsigned char> pixels;
    };

#if defined(IMIV_BACKEND_VULKAN_GLFW)

    struct PreviewControls {
        float exposure = 0.0f;
        float gamma    = 1.0f;
        int color_mode = 0;
        int channel    = 0;
        int use_ocio   = 0;
    };

    struct VulkanTexture {
        VkImage source_image         = VK_NULL_HANDLE;
        VkImageView source_view      = VK_NULL_HANDLE;
        VkDeviceMemory source_memory = VK_NULL_HANDLE;

        VkImage image         = VK_NULL_HANDLE;  // preview/output image
        VkImageView view      = VK_NULL_HANDLE;  // preview/output view
        VkDeviceMemory memory = VK_NULL_HANDLE;  // preview/output memory

        VkFramebuffer preview_framebuffer      = VK_NULL_HANDLE;
        VkDescriptorSet preview_source_set     = VK_NULL_HANDLE;
        VkSampler sampler     = VK_NULL_HANDLE;
        VkDescriptorSet set   = VK_NULL_HANDLE;
        int width             = 0;
        int height            = 0;
        bool preview_initialized = false;
        bool preview_dirty       = false;
        bool preview_params_valid = false;
        PreviewControls last_preview_controls = {};
    };

    struct UploadComputePushConstants {
        uint32_t width            = 0;
        uint32_t height           = 0;
        uint32_t row_pitch_bytes  = 0;
        uint32_t pixel_stride     = 0;
        uint32_t channel_count    = 0;
        uint32_t data_type        = 0;
    };

    struct PreviewPushConstants {
        float exposure   = 0.0f;
        float gamma      = 1.0f;
        int32_t color_mode = 0;
        int32_t channel    = 0;
        int32_t use_ocio   = 0;
    };

    struct VulkanState {
        VkAllocationCallbacks* allocator         = nullptr;
        VkInstance instance                      = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device         = VK_NULL_HANDLE;
        VkDevice device                          = VK_NULL_HANDLE;
        uint32_t queue_family                    = static_cast<uint32_t>(-1);
        VkQueueFamilyProperties queue_family_properties = {};
        VkQueue queue                                   = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache                  = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool                = VK_NULL_HANDLE;
        VkSurfaceKHR surface                            = VK_NULL_HANDLE;
        ImGui_ImplVulkanH_Window window_data;
        uint32_t min_image_count                                  = 2;
        bool swapchain_rebuild                                    = false;
        bool validation_layer_enabled                             = false;
        bool debug_utils_enabled                                  = false;
        bool queue_requires_full_image_copies                     = false;
        bool warned_about_full_imgui_uploads                      = false;
        bool compute_upload_ready                                 = false;
        bool compute_supports_float64                             = false;
        VkFormat compute_output_format                            = VK_FORMAT_UNDEFINED;
        VkDescriptorPool compute_descriptor_pool                  = VK_NULL_HANDLE;
        VkDescriptorSetLayout compute_descriptor_set_layout       = VK_NULL_HANDLE;
        VkPipelineLayout compute_pipeline_layout                  = VK_NULL_HANDLE;
        VkPipeline compute_pipeline                               = VK_NULL_HANDLE;
        VkPipeline compute_pipeline_fp64                          = VK_NULL_HANDLE;

        VkDescriptorPool preview_descriptor_pool                  = VK_NULL_HANDLE;
        VkDescriptorSetLayout preview_descriptor_set_layout       = VK_NULL_HANDLE;
        VkPipelineLayout preview_pipeline_layout                  = VK_NULL_HANDLE;
        VkPipeline preview_pipeline                               = VK_NULL_HANDLE;
        VkRenderPass preview_render_pass                          = VK_NULL_HANDLE;
        PFN_vkSetDebugUtilsObjectNameEXT set_debug_object_name_fn = nullptr;
    };

#endif

    struct ViewerState {
        LoadedImage image;
        std::string status_message;
        std::string last_error;
        float zoom       = 1.0f;
        bool fit_request = true;
        std::vector<std::string> sibling_images;
        int sibling_index = -1;
        std::string toggle_image_path;
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        VulkanTexture texture;
#endif
    };



    struct PlaceholderUiState {
        bool show_info_window        = false;
        bool show_preferences_window = false;
        bool show_pixelview_window   = false;
        bool show_area_probe_window  = false;
        bool show_status_window      = true;
        bool show_window_guides      = false;
        bool fit_image_to_window     = false;
        bool full_screen_mode        = false;
        bool slide_show_running      = false;
        bool slide_loop              = true;
        bool use_ocio                = false;
        bool pixelview_follows_mouse = false;
        bool linear_interpolation    = true;
        bool dark_palette            = true;
        bool auto_mipmap             = false;

        int max_memory_ic_mb       = 2048;
        int slide_duration_seconds = 10;
        int closeup_pixels         = 13;
        int closeup_avg_pixels     = 11;
        int current_channel        = 0;
        int subimage_index         = 0;
        int miplevel_index         = 0;
        int color_mode             = 0;

        float exposure = 0.0f;
        float gamma    = 1.0f;

        std::string ocio_display           = "default";
        std::string ocio_view              = "default";
        std::string ocio_image_color_space = "auto";
    };

    void refresh_sibling_images(ViewerState& viewer);
    bool pick_sibling_image(const ViewerState& viewer, int delta,
                            std::string& out_path);



    char to_lower_ascii(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c - 'A' + 'a');
        return c;
    }



    bool streq_i_ascii(const char* a, const char* b)
    {
        if (a == nullptr || b == nullptr)
            return false;
        while (*a && *b) {
            if (to_lower_ascii(*a) != to_lower_ascii(*b))
                return false;
            ++a;
            ++b;
        }
        return (*a == '\0' && *b == '\0');
    }



    bool read_env_value(const char* name, std::string& out_value)
    {
        out_value.clear();
#if defined(_WIN32)
        char* value       = nullptr;
        size_t value_size = 0;
        errno_t err       = _dupenv_s(&value, &value_size, name);
        if (err != 0 || value == nullptr || value_size == 0) {
            if (value != nullptr)
                std::free(value);
            return false;
        }
        out_value.assign(value);
        std::free(value);
#else
        const char* value = std::getenv(name);
        if (value == nullptr)
            return false;
        out_value.assign(value);
#endif
        return true;
    }



    bool env_flag_is_truthy(const char* name)
    {
        std::string value;
        if (!read_env_value(name, value))
            return false;

        const char* v = value.c_str();
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')
            ++v;
        if (*v == '\0')
            return false;
        if (*v == '1')
            return true;
        if (*v == '0')
            return false;
        return streq_i_ascii(v, "true") || streq_i_ascii(v, "yes")
               || streq_i_ascii(v, "on");
    }



    int env_int_value(const char* name, int fallback)
    {
        std::string value;
        if (!read_env_value(name, value) || value.empty())
            return fallback;
        char* end = nullptr;
        long x    = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str())
            return fallback;
        if (x < 0)
            return 0;
        if (x > 1000000)
            return 1000000;
        return static_cast<int>(x);
    }

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    struct TestEngineConfig {
        bool want_test_engine = false;
        bool trace            = false;
        bool auto_screenshot  = false;
        bool layout_dump      = false;
        bool junit_xml        = false;
        bool automation_mode  = false;
        bool exit_on_finish   = false;
        bool has_work         = false;
        bool show_windows     = false;
        std::string junit_xml_out;
    };

    struct TestEngineRuntime {
        ImGuiTestEngine* engine = nullptr;
        bool request_exit       = false;
        bool show_windows       = false;
    };
#endif


    void glfw_error_callback(int error, const char* description)
    {
        print(stderr, "imiv: GLFW error {}: {}\n", error, description);
    }

    bool load_image_for_compute(const std::string& path, LoadedImage& image,
                                std::string& error_message)
    {
        ImageBuf source(path);
        if (!source.read(0, 0, true, TypeUnknown)) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to read image";
            return false;
        }

        const ImageSpec& spec = source.spec();
        if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
            error_message = "image has invalid dimensions or channel count";
            return false;
        }

        UploadDataType upload_type = UploadDataType::Unknown;
        TypeDesc read_format       = TypeUnknown;
        if (!map_spec_type_to_upload(spec.format, upload_type, read_format)) {
            upload_type = UploadDataType::Float;
            read_format = TypeFloat;
        }

        const size_t width         = static_cast<size_t>(spec.width);
        const size_t height        = static_cast<size_t>(spec.height);
        const size_t channel_count = static_cast<size_t>(spec.nchannels);
        const size_t channel_bytes = upload_data_type_size(upload_type);
        if (channel_bytes == 0) {
            error_message = "unsupported pixel data type";
            return false;
        }
        const size_t row_pitch = width * channel_count * channel_bytes;
        const size_t total_size = row_pitch * height;

        std::vector<unsigned char> pixels(total_size);
        if (!source.get_pixels(source.roi(), read_format, pixels.data())) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to fetch pixel data";
            return false;
        }

        image.path        = path;
        image.width       = spec.width;
        image.height      = spec.height;
        image.nchannels   = spec.nchannels;
        image.type        = upload_type;
        image.channel_bytes   = channel_bytes;
        image.row_pitch_bytes = row_pitch;
        image.pixels      = std::move(pixels);

        if (spec.format != read_format) {
            print(stderr,
                  "imiv: source format '{}' converted to '{}' for compute upload path\n",
                  spec.format.c_str(), read_format.c_str());
        }
        return true;
    }



#if defined(IMIV_BACKEND_VULKAN_GLFW)

    void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
            return;
        print(stderr, "imiv: Vulkan error {}\n", static_cast<int>(err));
    }



    bool
    is_extension_available(const ImVector<VkExtensionProperties>& properties,
                           const char* extension_name)
    {
        for (const VkExtensionProperties& p : properties) {
            if (std::strcmp(p.extensionName, extension_name) == 0)
                return true;
        }
        return false;
    }



    void append_unique_extension(ImVector<const char*>& extensions,
                                 const char* extension_name)
    {
        for (const char* existing_name : extensions) {
            if (std::strcmp(existing_name, extension_name) == 0)
                return;
        }
        extensions.push_back(extension_name);
    }



    bool is_layer_available(const char* layer_name)
    {
        uint32_t count = 0;
        VkResult err   = vkEnumerateInstanceLayerProperties(&count, nullptr);
        if (err != VK_SUCCESS)
            return false;

        std::vector<VkLayerProperties> properties(count);
        err = vkEnumerateInstanceLayerProperties(&count, properties.data());
        if (err != VK_SUCCESS)
            return false;

        for (const VkLayerProperties& p : properties) {
            if (std::strcmp(p.layerName, layer_name) == 0)
                return true;
        }
        return false;
    }



    const char* severity_name(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
    {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            return "error";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            return "warning";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            return "info";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            return "verbose";
        return "unknown";
    }



    void print_message_type(VkDebugUtilsMessageTypeFlagsEXT message_type)
    {
        bool first = true;

        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            print(stderr, "general");
            first = false;
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "validation");
            first = false;
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "performance");
            first = false;
        }
#    ifdef VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
        if (message_type
            & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "device_address_binding");
            first = false;
        }
#    endif

        if (first)
            print(stderr, "unknown");
    }



    const char* object_type_name(VkObjectType object_type)
    {
        switch (object_type) {
        case VK_OBJECT_TYPE_INSTANCE: return "instance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE: return "physical_device";
        case VK_OBJECT_TYPE_DEVICE: return "device";
        case VK_OBJECT_TYPE_QUEUE: return "queue";
        case VK_OBJECT_TYPE_SEMAPHORE: return "semaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER: return "command_buffer";
        case VK_OBJECT_TYPE_FENCE: return "fence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY: return "device_memory";
        case VK_OBJECT_TYPE_BUFFER: return "buffer";
        case VK_OBJECT_TYPE_IMAGE: return "image";
        case VK_OBJECT_TYPE_EVENT: return "event";
        case VK_OBJECT_TYPE_QUERY_POOL: return "query_pool";
        case VK_OBJECT_TYPE_BUFFER_VIEW: return "buffer_view";
        case VK_OBJECT_TYPE_IMAGE_VIEW: return "image_view";
        case VK_OBJECT_TYPE_SHADER_MODULE: return "shader_module";
        case VK_OBJECT_TYPE_PIPELINE_CACHE: return "pipeline_cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "pipeline_layout";
        case VK_OBJECT_TYPE_RENDER_PASS: return "render_pass";
        case VK_OBJECT_TYPE_PIPELINE: return "pipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "descriptor_set_layout";
        case VK_OBJECT_TYPE_SAMPLER: return "sampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "descriptor_pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "descriptor_set";
        case VK_OBJECT_TYPE_FRAMEBUFFER: return "framebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL: return "command_pool";
        case VK_OBJECT_TYPE_SURFACE_KHR: return "surface";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "swapchain";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return "debug_utils_messenger";
        default: break;
        }
        return "unknown";
    }



    VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data)
    {
        (void)user_data;

        const char* message_id_name = "";
        const char* message         = "";
        int32_t message_id_number   = 0;
        if (callback_data != nullptr) {
            if (callback_data->pMessageIdName != nullptr)
                message_id_name = callback_data->pMessageIdName;
            if (callback_data->pMessage != nullptr)
                message = callback_data->pMessage;
            message_id_number = callback_data->messageIdNumber;
        }

        print(stderr, "imiv: vk[{}][", severity_name(severity));
        print_message_type(message_type);
        print(stderr, "] id={} {}: {}\n", message_id_number, message_id_name,
              message);

        const bool print_objects
            = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
              || (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
        if (print_objects && callback_data != nullptr
            && callback_data->objectCount > 0) {
            for (uint32_t i = 0; i < callback_data->objectCount; ++i) {
                const VkDebugUtilsObjectNameInfoEXT& o
                    = callback_data->pObjects[i];
                const char* object_name = (o.pObjectName != nullptr
                                           && o.pObjectName[0] != '\0')
                                              ? o.pObjectName
                                              : "<unnamed>";
                print(stderr, "imiv: vk object[{}] type={} handle={} name={}\n",
                      i, object_type_name(o.objectType), o.objectHandle,
                      object_name);
            }
        }
        return VK_FALSE;
    }



    const char* texture_status_name(ImTextureStatus status)
    {
        switch (status) {
        case ImTextureStatus_OK: return "ok";
        case ImTextureStatus_Destroyed: return "destroyed";
        case ImTextureStatus_WantCreate: return "want_create";
        case ImTextureStatus_WantUpdates: return "want_updates";
        case ImTextureStatus_WantDestroy: return "want_destroy";
        default: break;
        }
        return "unknown";
    }



    std::string queue_flags_string(VkQueueFlags flags)
    {
        std::string out;
        if (flags & VK_QUEUE_GRAPHICS_BIT) {
            if (!out.empty())
                out += "|";
            out += "graphics";
        }
        if (flags & VK_QUEUE_COMPUTE_BIT) {
            if (!out.empty())
                out += "|";
            out += "compute";
        }
        if (flags & VK_QUEUE_TRANSFER_BIT) {
            if (!out.empty())
                out += "|";
            out += "transfer";
        }
        if (flags & VK_QUEUE_SPARSE_BINDING_BIT) {
            if (!out.empty())
                out += "|";
            out += "sparse";
        }
#    ifdef VK_QUEUE_PROTECTED_BIT
        if (flags & VK_QUEUE_PROTECTED_BIT) {
            if (!out.empty())
                out += "|";
            out += "protected";
        }
#    endif
        if (out.empty())
            out = "none";
        return out;
    }



    bool cache_queue_family_properties(VulkanState& vk_state,
                                       std::string& error_message)
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vk_state.physical_device,
                                                 &queue_family_count, nullptr);
        if (queue_family_count == 0) {
            error_message
                = "vkGetPhysicalDeviceQueueFamilyProperties found no queue families";
            return false;
        }
        if (vk_state.queue_family >= queue_family_count) {
            error_message = "selected queue family index is out of range";
            return false;
        }

        std::vector<VkQueueFamilyProperties> properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(vk_state.physical_device,
                                                 &queue_family_count,
                                                 properties.data());
        vk_state.queue_family_properties = properties[vk_state.queue_family];

        const VkExtent3D granularity
            = vk_state.queue_family_properties.minImageTransferGranularity;
        vk_state.queue_requires_full_image_copies = (granularity.width == 0
                                                     || granularity.height == 0
                                                     || granularity.depth == 0);

        print("imiv: Vulkan queue family {} flags={} granularity=({}, {}, {}) "
              "count={}\n",
              vk_state.queue_family,
              queue_flags_string(vk_state.queue_family_properties.queueFlags),
              granularity.width, granularity.height, granularity.depth,
              vk_state.queue_family_properties.queueCount);
        if (vk_state.queue_requires_full_image_copies) {
            print(stderr,
                  "imiv: queue family {} requires full-image transfer copies; "
                  "enabling conservative ImGui texture upload workaround\n",
                  vk_state.queue_family);
        }
        return true;
    }



    template<typename HandleT> uint64_t vk_handle_to_u64(HandleT handle)
    {
        if constexpr (std::is_pointer<HandleT>::value) {
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
        } else {
            return static_cast<uint64_t>(handle);
        }
    }



    template<typename HandleT>
    void set_vk_object_name(VulkanState& vk_state, VkObjectType object_type,
                            HandleT handle, const char* name)
    {
        if (vk_state.set_debug_object_name_fn == nullptr
            || handle == VK_NULL_HANDLE || name == nullptr || name[0] == '\0')
            return;

        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType   = object_type;
        info.objectHandle = vk_handle_to_u64(handle);
        info.pObjectName  = name;
        vk_state.set_debug_object_name_fn(vk_state.device, &info);
    }



    void name_window_frame_objects(VulkanState& vk_state)
    {
        if (vk_state.set_debug_object_name_fn == nullptr)
            return;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SWAPCHAIN_KHR,
                           wd->Swapchain, "imiv.main.swapchain");
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS, wd->RenderPass,
                           "imiv.main.render_pass");

        for (int i = 0; i < wd->Frames.Size; ++i) {
            char buffer_name[64] = {};
            std::snprintf(buffer_name, sizeof(buffer_name),
                          "imiv.main.frame[%d].command_buffer", i);
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               wd->Frames[i].CommandBuffer, buffer_name);

            char image_name[64] = {};
            std::snprintf(image_name, sizeof(image_name),
                          "imiv.main.frame[%d].backbuffer", i);
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE,
                               wd->Frames[i].Backbuffer, image_name);
        }
    }



    unsigned short clamp_u16(int value)
    {
        if (value <= 0)
            return 0;
        if (value
            > static_cast<int>(std::numeric_limits<unsigned short>::max())) {
            return std::numeric_limits<unsigned short>::max();
        }
        return static_cast<unsigned short>(value);
    }



    void apply_imgui_texture_update_workarounds(VulkanState& vk_state,
                                                ImDrawData* draw_data)
    {
#    if defined(IMGUI_HAS_TEXTURES)
        if (draw_data == nullptr || draw_data->Textures == nullptr)
            return;

        for (ImTextureData* tex : *draw_data->Textures) {
            if (tex == nullptr || tex->Status == ImTextureStatus_OK)
                continue;

            print(
                stderr,
                "imiv: imgui texture id={} status={} size={}x{} update=({},{} {}x{}) pending={}\n",
                tex->UniqueID, texture_status_name(tex->Status), tex->Width,
                tex->Height, tex->UpdateRect.x, tex->UpdateRect.y,
                tex->UpdateRect.w, tex->UpdateRect.h, tex->Updates.Size);

            if (!vk_state.queue_requires_full_image_copies
                || tex->Status != ImTextureStatus_WantUpdates) {
                continue;
            }

            ImTextureRect full_rect = {};
            full_rect.x             = 0;
            full_rect.y             = 0;
            full_rect.w             = clamp_u16(tex->Width);
            full_rect.h             = clamp_u16(tex->Height);
            tex->UpdateRect         = full_rect;
            tex->Updates.resize(1);
            tex->Updates[0] = full_rect;

            if (!vk_state.warned_about_full_imgui_uploads) {
                print(stderr,
                      "imiv: forcing full ImGui texture uploads on this queue "
                      "family due to strict transfer granularity\n");
                vk_state.warned_about_full_imgui_uploads = true;
            }
        }
#    else
        (void)vk_state;
        (void)draw_data;
#    endif
    }



    void populate_debug_messenger_ci(VkDebugUtilsMessengerCreateInfoEXT& ci)
    {
        ci       = {};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = vulkan_debug_callback;
    }



    bool setup_debug_messenger(VulkanState& vk_state,
                               std::string& error_message)
    {
        PFN_vkCreateDebugUtilsMessengerEXT create_fn
            = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vk_state.instance,
                                      "vkCreateDebugUtilsMessengerEXT"));
        if (create_fn == nullptr) {
            error_message = "vkCreateDebugUtilsMessengerEXT not available";
            return false;
        }

        VkDebugUtilsMessengerCreateInfoEXT ci = {};
        populate_debug_messenger_ci(ci);
        VkResult err = create_fn(vk_state.instance, &ci, vk_state.allocator,
                                 &vk_state.debug_messenger);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDebugUtilsMessengerEXT failed";
            return false;
        }
        return true;
    }



    bool find_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits,
                          VkMemoryPropertyFlags required,
                          uint32_t& memory_type_index)
    {
        VkPhysicalDeviceMemoryProperties memory_properties = {};
        vkGetPhysicalDeviceMemoryProperties(physical_device,
                                            &memory_properties);
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
            const bool type_matches = (type_bits & (1u << i)) != 0;
            const bool has_flags
                = (memory_properties.memoryTypes[i].propertyFlags & required)
                  == required;
            if (type_matches && has_flags) {
                memory_type_index = i;
                return true;
            }
        }
        return false;
    }



    bool has_format_features(VkPhysicalDevice physical_device, VkFormat format,
                             VkFormatFeatureFlags required_features)
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
        return (props.optimalTilingFeatures & required_features)
               == required_features;
    }



    bool read_binary_file(const std::string& path, std::vector<uint32_t>& words,
                          std::string& error_message)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) {
            error_message = Strutil::fmt::format("cannot open '{}'", path);
            return false;
        }

        const std::streamsize size = in.tellg();
        if (size <= 0 || (size % 4) != 0) {
            error_message = Strutil::fmt::format(
                "invalid SPIR-V file size for '{}'", path);
            return false;
        }
        in.seekg(0, std::ios::beg);

        words.resize(static_cast<size_t>(size) / 4);
        if (!in.read(reinterpret_cast<char*>(words.data()), size)) {
            error_message = Strutil::fmt::format("failed to read '{}'", path);
            words.clear();
            return false;
        }
        return true;
    }



    bool create_compute_pipeline_from_file(VulkanState& vk_state,
                                           const std::string& shader_path,
                                           const char* debug_name,
                                           VkPipeline& out_pipeline,
                                           std::string& error_message)
    {
        out_pipeline = VK_NULL_HANDLE;
        std::vector<uint32_t> shader_words;
        if (!read_binary_file(shader_path, shader_words, error_message))
            return false;

        VkShaderModule shader_module = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo shader_ci = {};
        shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_ci.codeSize = shader_words.size() * sizeof(uint32_t);
        shader_ci.pCode    = shader_words.data();
        VkResult err = vkCreateShaderModule(vk_state.device, &shader_ci,
                                            vk_state.allocator, &shader_module);
        if (err != VK_SUCCESS) {
            error_message = Strutil::fmt::format(
                "vkCreateShaderModule failed for '{}'", shader_path);
            return false;
        }

        VkPipelineShaderStageCreateInfo stage_ci = {};
        stage_ci.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_ci.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_ci.module = shader_module;
        stage_ci.pName  = "main";
        VkComputePipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_ci.stage  = stage_ci;
        pipeline_ci.layout = vk_state.compute_pipeline_layout;
        err = vkCreateComputePipelines(vk_state.device, vk_state.pipeline_cache,
                                       1, &pipeline_ci, vk_state.allocator,
                                       &out_pipeline);
        vkDestroyShaderModule(vk_state.device, shader_module, vk_state.allocator);
        if (err != VK_SUCCESS) {
            error_message = Strutil::fmt::format(
                "vkCreateComputePipelines failed for '{}'", shader_path);
            out_pipeline = VK_NULL_HANDLE;
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE, out_pipeline,
                           debug_name);
        return true;
    }



    bool create_shader_module_from_file(VkDevice device,
                                        VkAllocationCallbacks* allocator,
                                        const std::string& shader_path,
                                        VkShaderModule& shader_module,
                                        std::string& error_message)
    {
        shader_module = VK_NULL_HANDLE;
        std::vector<uint32_t> shader_words;
        if (!read_binary_file(shader_path, shader_words, error_message))
            return false;

        VkShaderModuleCreateInfo shader_ci = {};
        shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_ci.codeSize = shader_words.size() * sizeof(uint32_t);
        shader_ci.pCode    = shader_words.data();
        VkResult err = vkCreateShaderModule(device, &shader_ci, allocator,
                                            &shader_module);
        if (err != VK_SUCCESS) {
            error_message = Strutil::fmt::format(
                "vkCreateShaderModule failed for '{}'", shader_path);
            shader_module = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }



    void destroy_preview_resources(VulkanState& vk_state)
    {
        if (vk_state.preview_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.preview_pipeline,
                              vk_state.allocator);
            vk_state.preview_pipeline = VK_NULL_HANDLE;
        }
        if (vk_state.preview_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vk_state.device,
                                    vk_state.preview_pipeline_layout,
                                    vk_state.allocator);
            vk_state.preview_pipeline_layout = VK_NULL_HANDLE;
        }
        if (vk_state.preview_descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vk_state.device,
                                         vk_state.preview_descriptor_set_layout,
                                         vk_state.allocator);
            vk_state.preview_descriptor_set_layout = VK_NULL_HANDLE;
        }
        if (vk_state.preview_descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device,
                                    vk_state.preview_descriptor_pool,
                                    vk_state.allocator);
            vk_state.preview_descriptor_pool = VK_NULL_HANDLE;
        }
        if (vk_state.preview_render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(vk_state.device, vk_state.preview_render_pass,
                                vk_state.allocator);
            vk_state.preview_render_pass = VK_NULL_HANDLE;
        }
    }



    bool init_preview_resources(VulkanState& vk_state, std::string& error_message)
    {
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
        error_message = "preview shaders were not generated at build time";
        return false;
#    else
        destroy_preview_resources(vk_state);

        if (!has_format_features(vk_state.physical_device,
                                 vk_state.compute_output_format,
                                 VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
            error_message
                = "selected preview output format does not support color attachment";
            return false;
        }

        VkAttachmentDescription attachment = {};
        attachment.format         = vk_state.compute_output_format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref = {};
        color_ref.attachment = 0;
        color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_ref;

        VkRenderPassCreateInfo render_pass_ci = {};
        render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_ci.attachmentCount = 1;
        render_pass_ci.pAttachments    = &attachment;
        render_pass_ci.subpassCount    = 1;
        render_pass_ci.pSubpasses      = &subpass;
        VkResult err = vkCreateRenderPass(vk_state.device, &render_pass_ci,
                                          vk_state.allocator,
                                          &vk_state.preview_render_pass);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateRenderPass failed for preview pipeline";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS,
                           vk_state.preview_render_pass,
                           "imiv.preview.render_pass");

        VkDescriptorPoolSize preview_pool_size = {};
        preview_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preview_pool_size.descriptorCount = 64;
        VkDescriptorPoolCreateInfo preview_pool_ci = {};
        preview_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        preview_pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        preview_pool_ci.maxSets = 64;
        preview_pool_ci.poolSizeCount = 1;
        preview_pool_ci.pPoolSizes = &preview_pool_size;
        err = vkCreateDescriptorPool(vk_state.device, &preview_pool_ci,
                                     vk_state.allocator,
                                     &vk_state.preview_descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.preview_descriptor_pool,
                           "imiv.preview.descriptor_pool");

        VkDescriptorSetLayoutBinding preview_binding = {};
        preview_binding.binding            = 0;
        preview_binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preview_binding.descriptorCount    = 1;
        preview_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo preview_set_layout_ci = {};
        preview_set_layout_ci.sType
            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        preview_set_layout_ci.bindingCount = 1;
        preview_set_layout_ci.pBindings    = &preview_binding;
        err = vkCreateDescriptorSetLayout(vk_state.device,
                                          &preview_set_layout_ci,
                                          vk_state.allocator,
                                          &vk_state.preview_descriptor_set_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorSetLayout failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           vk_state.preview_descriptor_set_layout,
                           "imiv.preview.set_layout");

        VkPushConstantRange preview_push = {};
        preview_push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        preview_push.offset     = 0;
        preview_push.size       = sizeof(PreviewPushConstants);

        VkPipelineLayoutCreateInfo preview_layout_ci = {};
        preview_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        preview_layout_ci.setLayoutCount = 1;
        preview_layout_ci.pSetLayouts    = &vk_state.preview_descriptor_set_layout;
        preview_layout_ci.pushConstantRangeCount = 1;
        preview_layout_ci.pPushConstantRanges    = &preview_push;
        err = vkCreatePipelineLayout(vk_state.device, &preview_layout_ci,
                                     vk_state.allocator,
                                     &vk_state.preview_pipeline_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreatePipelineLayout failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           vk_state.preview_pipeline_layout,
                           "imiv.preview.pipeline_layout");

        const std::string shader_vert
            = std::string(IMIV_SHADER_DIR) + "/imiv_preview.vert.spv";
        const std::string shader_frag
            = std::string(IMIV_SHADER_DIR) + "/imiv_preview.frag.spv";
        VkShaderModule vert_module = VK_NULL_HANDLE;
        VkShaderModule frag_module = VK_NULL_HANDLE;
        if (!create_shader_module_from_file(vk_state.device, vk_state.allocator,
                                            shader_vert, vert_module,
                                            error_message)) {
            destroy_preview_resources(vk_state);
            return false;
        }
        if (!create_shader_module_from_file(vk_state.device, vk_state.allocator,
                                            shader_frag, frag_module,
                                            error_message)) {
            vkDestroyShaderModule(vk_state.device, vert_module,
                                  vk_state.allocator);
            destroy_preview_resources(vk_state);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_module;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input = {};
        vertex_input.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType
            = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount  = 1;
        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType
            = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                | VK_COLOR_COMPONENT_G_BIT
                                                | VK_COLOR_COMPONENT_B_BIT
                                                | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo color_blend = {};
        color_blend.sType
            = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount = 1;
        color_blend.pAttachments    = &color_blend_attachment;
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType
            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount
            = static_cast<uint32_t>(IM_ARRAYSIZE(dynamic_states));
        dynamic_state.pDynamicStates = dynamic_states;

        VkGraphicsPipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.stageCount = 2;
        pipeline_ci.pStages    = stages;
        pipeline_ci.pVertexInputState   = &vertex_input;
        pipeline_ci.pInputAssemblyState = &input_assembly;
        pipeline_ci.pViewportState      = &viewport_state;
        pipeline_ci.pRasterizationState = &raster;
        pipeline_ci.pMultisampleState   = &multisample;
        pipeline_ci.pColorBlendState    = &color_blend;
        pipeline_ci.pDynamicState       = &dynamic_state;
        pipeline_ci.layout              = vk_state.preview_pipeline_layout;
        pipeline_ci.renderPass          = vk_state.preview_render_pass;
        pipeline_ci.subpass             = 0;

        err = vkCreateGraphicsPipelines(vk_state.device, vk_state.pipeline_cache,
                                        1, &pipeline_ci, vk_state.allocator,
                                        &vk_state.preview_pipeline);
        vkDestroyShaderModule(vk_state.device, vert_module, vk_state.allocator);
        vkDestroyShaderModule(vk_state.device, frag_module, vk_state.allocator);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateGraphicsPipelines failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE,
                           vk_state.preview_pipeline,
                           "imiv.preview.pipeline");

        return true;
#    endif
    }



    void destroy_compute_upload_resources(VulkanState& vk_state)
    {
        if (vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.compute_pipeline_fp64,
                              vk_state.allocator);
            vk_state.compute_pipeline_fp64 = VK_NULL_HANDLE;
        }
        if (vk_state.compute_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.compute_pipeline,
                              vk_state.allocator);
            vk_state.compute_pipeline = VK_NULL_HANDLE;
        }
        if (vk_state.compute_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vk_state.device,
                                    vk_state.compute_pipeline_layout,
                                    vk_state.allocator);
            vk_state.compute_pipeline_layout = VK_NULL_HANDLE;
        }
        if (vk_state.compute_descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vk_state.device,
                                         vk_state.compute_descriptor_set_layout,
                                         vk_state.allocator);
            vk_state.compute_descriptor_set_layout = VK_NULL_HANDLE;
        }
        if (vk_state.compute_descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device,
                                    vk_state.compute_descriptor_pool,
                                    vk_state.allocator);
            vk_state.compute_descriptor_pool = VK_NULL_HANDLE;
        }
        vk_state.compute_output_format    = VK_FORMAT_UNDEFINED;
        vk_state.compute_upload_ready     = false;
    }



    bool init_compute_upload_resources(VulkanState& vk_state,
                                       std::string& error_message)
    {
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
        error_message = "compute upload shaders were not generated at build time";
        return false;
#    else
        destroy_compute_upload_resources(vk_state);

        if ((vk_state.queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
            == 0) {
            error_message
                = "selected Vulkan queue family does not support compute";
            return false;
        }

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(vk_state.physical_device, &features);
        vk_state.compute_supports_float64 = (features.shaderFloat64 == VK_TRUE);

        const VkFormatFeatureFlags required
            = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
              | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

        std::string shader_path;
        std::string shader_path_fp64;
        if (has_format_features(vk_state.physical_device,
                                VK_FORMAT_R16G16B16A16_SFLOAT, required)) {
            vk_state.compute_output_format = VK_FORMAT_R16G16B16A16_SFLOAT;
            shader_path
                = std::string(IMIV_SHADER_DIR) + "/imiv_upload_to_rgba16f.comp.spv";
            shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                               + "/imiv_upload_to_rgba16f_fp64.comp.spv";
        } else if (has_format_features(vk_state.physical_device,
                                       VK_FORMAT_R32G32B32A32_SFLOAT,
                                       required)) {
            vk_state.compute_output_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            shader_path
                = std::string(IMIV_SHADER_DIR) + "/imiv_upload_to_rgba32f.comp.spv";
            shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                               + "/imiv_upload_to_rgba32f_fp64.comp.spv";
        } else {
            error_message
                = "no compute output format support for rgba16f/rgba32f storage image";
            return false;
        }

        VkDescriptorPoolSize pool_sizes[2] = {};
        pool_sizes[0].type                 = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[0].descriptorCount      = 64;
        pool_sizes[1].type                 = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_sizes[1].descriptorCount      = 64;
        VkDescriptorPoolCreateInfo pool_ci = {};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_ci.maxSets       = 64;
        pool_ci.poolSizeCount = 2;
        pool_ci.pPoolSizes    = pool_sizes;
        VkResult err
            = vkCreateDescriptorPool(vk_state.device, &pool_ci,
                                     vk_state.allocator,
                                     &vk_state.compute_descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.compute_descriptor_pool,
                           "imiv.compute_upload.descriptor_pool");

        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo set_layout_ci = {};
        set_layout_ci.sType
            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_layout_ci.bindingCount = 2;
        set_layout_ci.pBindings    = bindings;
        err = vkCreateDescriptorSetLayout(vk_state.device, &set_layout_ci,
                                          vk_state.allocator,
                                          &vk_state.compute_descriptor_set_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorSetLayout failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           vk_state.compute_descriptor_set_layout,
                           "imiv.compute_upload.set_layout");

        VkPushConstantRange push_range = {};
        push_range.stageFlags          = VK_SHADER_STAGE_COMPUTE_BIT;
        push_range.offset              = 0;
        push_range.size = sizeof(UploadComputePushConstants);
        VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
        pipeline_layout_ci.sType
            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_ci.setLayoutCount = 1;
        pipeline_layout_ci.pSetLayouts
            = &vk_state.compute_descriptor_set_layout;
        pipeline_layout_ci.pushConstantRangeCount = 1;
        pipeline_layout_ci.pPushConstantRanges    = &push_range;
        err = vkCreatePipelineLayout(vk_state.device, &pipeline_layout_ci,
                                     vk_state.allocator,
                                     &vk_state.compute_pipeline_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreatePipelineLayout failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           vk_state.compute_pipeline_layout,
                           "imiv.compute_upload.pipeline_layout");

        if (!create_compute_pipeline_from_file(vk_state, shader_path,
                                               "imiv.compute_upload.pipeline",
                                               vk_state.compute_pipeline,
                                               error_message)) {
            destroy_compute_upload_resources(vk_state);
            return false;
        }

        if (vk_state.compute_supports_float64) {
            std::string fp64_error;
            if (!create_compute_pipeline_from_file(
                    vk_state, shader_path_fp64,
                    "imiv.compute_upload.pipeline_fp64",
                    vk_state.compute_pipeline_fp64, fp64_error)) {
                print(stderr,
                      "imiv: fp64 compute pipeline unavailable, will fallback "
                      "double->float on CPU: {}\n",
                      fp64_error);
            }
        }

        vk_state.compute_upload_ready = true;
        print(
            "imiv: compute upload ready output_format={} shader={} float64_support={} fp64_pipeline={}\n",
              static_cast<int>(vk_state.compute_output_format), shader_path,
              vk_state.compute_supports_float64 ? "yes" : "no",
              vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE ? "yes" : "no");
        return true;
#    endif
    }



    bool setup_vulkan(VulkanState& vk_state,
                      ImVector<const char*>& instance_extensions,
                      std::string& error_message)
    {
        VkResult err;

        VkInstanceCreateInfo instance_ci = {};
        instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ImVector<const char*> instance_layers;

#    if defined(IMIV_VULKAN_VALIDATION) && IMIV_VULKAN_VALIDATION
        if (is_layer_available("VK_LAYER_KHRONOS_validation")) {
            vk_state.validation_layer_enabled = true;
            instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        } else {
            print(
                stderr,
                "imiv: Vulkan validation requested but VK_LAYER_KHRONOS_validation was not found\n");
        }
#    endif

        uint32_t extension_count = 0;
        err = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                                     nullptr);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateInstanceExtensionProperties failed";
            return false;
        }

        ImVector<VkExtensionProperties> instance_properties;
        instance_properties.resize(extension_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                                     instance_properties.Data);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateInstanceExtensionProperties failed";
            return false;
        }

        if (is_extension_available(
                instance_properties,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            append_unique_extension(
                instance_extensions,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#    ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (is_extension_available(
                instance_properties,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            append_unique_extension(
                instance_extensions,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instance_ci.flags
                |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#    endif
        if (vk_state.validation_layer_enabled) {
            if (is_extension_available(instance_properties,
                                       VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                append_unique_extension(instance_extensions,
                                        VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                vk_state.debug_utils_enabled = true;
            } else {
                print(
                    stderr,
                    "imiv: VK_EXT_debug_utils not found; validation output will be limited\n");
            }
        }

        VkApplicationInfo app_info        = {};
        app_info.sType                    = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName         = "imiv";
        app_info.applicationVersion       = VK_MAKE_API_VERSION(0, 0, 1, 0);
        app_info.pEngineName              = "OpenImageIO";
        app_info.engineVersion            = VK_MAKE_API_VERSION(0, 0, 1, 0);
        app_info.apiVersion               = VK_API_VERSION_1_2;
        instance_ci.pApplicationInfo      = &app_info;
        instance_ci.enabledExtensionCount = static_cast<uint32_t>(
            instance_extensions.Size);
        instance_ci.ppEnabledExtensionNames = instance_extensions.Data;
        instance_ci.enabledLayerCount       = static_cast<uint32_t>(
            instance_layers.Size);
        instance_ci.ppEnabledLayerNames = instance_layers.Data;

        VkDebugUtilsMessengerCreateInfoEXT debug_ci = {};
        if (vk_state.validation_layer_enabled && vk_state.debug_utils_enabled) {
            populate_debug_messenger_ci(debug_ci);
            instance_ci.pNext = &debug_ci;
        }

        err = vkCreateInstance(&instance_ci, vk_state.allocator,
                               &vk_state.instance);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateInstance failed";
            return false;
        }
        if (vk_state.validation_layer_enabled) {
            if (vk_state.debug_utils_enabled) {
                if (!setup_debug_messenger(vk_state, error_message))
                    return false;
                print(
                    "imiv: Vulkan validation enabled with verbose debug utils output\n");
            } else {
                print(
                    "imiv: Vulkan validation enabled without debug utils messenger\n");
            }
        }

        vk_state.physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(
            vk_state.instance);
        if (vk_state.physical_device == VK_NULL_HANDLE) {
            error_message = "no Vulkan physical device found";
            return false;
        }

        vk_state.queue_family = ImGui_ImplVulkanH_SelectQueueFamilyIndex(
            vk_state.physical_device);
        if (vk_state.queue_family == static_cast<uint32_t>(-1)) {
            error_message = "no suitable Vulkan queue family found";
            return false;
        }
        if (!cache_queue_family_properties(vk_state, error_message))
            return false;

        ImVector<const char*> device_extensions;
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        uint32_t device_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(vk_state.physical_device, nullptr,
                                             &device_extension_count, nullptr);
        ImVector<VkExtensionProperties> device_properties;
        device_properties.resize(device_extension_count);
        vkEnumerateDeviceExtensionProperties(vk_state.physical_device, nullptr,
                                             &device_extension_count,
                                             device_properties.Data);
#    ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (is_extension_available(device_properties,
                                   VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(
                VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#    endif

        const float queue_priority[]     = { 1.0f };
        VkDeviceQueueCreateInfo queue_ci = {};
        queue_ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = vk_state.queue_family;
        queue_ci.queueCount       = 1;
        queue_ci.pQueuePriorities = queue_priority;

        VkPhysicalDeviceFeatures supported_features = {};
        vkGetPhysicalDeviceFeatures(vk_state.physical_device,
                                    &supported_features);
        VkPhysicalDeviceFeatures enabled_features = {};
        if (supported_features.shaderFloat64 == VK_TRUE) {
            enabled_features.shaderFloat64  = VK_TRUE;
            vk_state.compute_supports_float64 = true;
        } else {
            vk_state.compute_supports_float64 = false;
        }

        VkDeviceCreateInfo device_ci    = {};
        device_ci.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.queueCreateInfoCount  = 1;
        device_ci.pQueueCreateInfos     = &queue_ci;
        device_ci.enabledExtensionCount = static_cast<uint32_t>(
            device_extensions.Size);
        device_ci.ppEnabledExtensionNames = device_extensions.Data;
        device_ci.pEnabledFeatures        = &enabled_features;
        err = vkCreateDevice(vk_state.physical_device, &device_ci,
                             vk_state.allocator, &vk_state.device);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDevice failed";
            return false;
        }

        if (vk_state.debug_utils_enabled) {
            vk_state.set_debug_object_name_fn
                = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetDeviceProcAddr(vk_state.device,
                                        "vkSetDebugUtilsObjectNameEXT"));
            if (vk_state.set_debug_object_name_fn == nullptr) {
                print(
                    stderr,
                    "imiv: vkSetDebugUtilsObjectNameEXT not available on device\n");
            }
        }

        vkGetDeviceQueue(vk_state.device, vk_state.queue_family, 0,
                         &vk_state.queue);
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_QUEUE, vk_state.queue,
                           "imiv.main.queue");

        VkDescriptorPoolSize pool_sizes[]
            = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 } };
        VkDescriptorPoolCreateInfo pool_ci = {};
        pool_ci.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_ci.maxSets = 256;
        pool_ci.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_ci.pPoolSizes    = pool_sizes;
        err = vkCreateDescriptorPool(vk_state.device, &pool_ci,
                                     vk_state.allocator,
                                     &vk_state.descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed";
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.descriptor_pool,
                           "imiv.main.descriptor_pool");

        if (!init_compute_upload_resources(vk_state, error_message))
            return false;
        if (!init_preview_resources(vk_state, error_message))
            return false;

        return true;
    }



    bool setup_vulkan_window(VulkanState& vk_state, int width, int height,
                             std::string& error_message)
    {
        VkBool32 has_wsi = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_state.physical_device,
                                             vk_state.queue_family,
                                             vk_state.surface, &has_wsi);
        if (has_wsi != VK_TRUE) {
            error_message = "no WSI support on selected device";
            return false;
        }

        const VkFormat request_surface_formats[]
            = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR request_color_space
            = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        vk_state.window_data.Surface = vk_state.surface;
        vk_state.window_data.SurfaceFormat
            = ImGui_ImplVulkanH_SelectSurfaceFormat(
                vk_state.physical_device, vk_state.window_data.Surface,
                request_surface_formats,
                static_cast<int>(IM_ARRAYSIZE(request_surface_formats)),
                request_color_space);

        const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
        vk_state.window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
            vk_state.physical_device, vk_state.window_data.Surface,
            present_modes, static_cast<int>(IM_ARRAYSIZE(present_modes)));

        ImGui_ImplVulkanH_CreateOrResizeWindow(
            vk_state.instance, vk_state.physical_device, vk_state.device,
            &vk_state.window_data, vk_state.queue_family, vk_state.allocator,
            width, height, vk_state.min_image_count,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        name_window_frame_objects(vk_state);
        return true;
    }



    void cleanup_vulkan_window(VulkanState& vk_state)
    {
        ImGui_ImplVulkanH_DestroyWindow(vk_state.instance, vk_state.device,
                                        &vk_state.window_data,
                                        vk_state.allocator);
        if (vk_state.surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(vk_state.instance, vk_state.surface,
                                vk_state.allocator);
            vk_state.surface = VK_NULL_HANDLE;
        }
    }



    void cleanup_vulkan(VulkanState& vk_state)
    {
        if (vk_state.debug_messenger != VK_NULL_HANDLE) {
            PFN_vkDestroyDebugUtilsMessengerEXT destroy_fn
                = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(vk_state.instance,
                                          "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy_fn != nullptr) {
                destroy_fn(vk_state.instance, vk_state.debug_messenger,
                           vk_state.allocator);
            }
            vk_state.debug_messenger = VK_NULL_HANDLE;
        }
        if (vk_state.device != VK_NULL_HANDLE)
            destroy_compute_upload_resources(vk_state);
        if (vk_state.device != VK_NULL_HANDLE)
            destroy_preview_resources(vk_state);
        if (vk_state.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device, vk_state.descriptor_pool,
                                    vk_state.allocator);
            vk_state.descriptor_pool = VK_NULL_HANDLE;
        }
        if (vk_state.pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(vk_state.device, vk_state.pipeline_cache,
                                   vk_state.allocator);
            vk_state.pipeline_cache = VK_NULL_HANDLE;
        }
        if (vk_state.device != VK_NULL_HANDLE) {
            vkDestroyDevice(vk_state.device, vk_state.allocator);
            vk_state.device = VK_NULL_HANDLE;
        }
        if (vk_state.instance != VK_NULL_HANDLE) {
            vkDestroyInstance(vk_state.instance, vk_state.allocator);
            vk_state.instance = VK_NULL_HANDLE;
        }
    }



    void frame_render(VulkanState& vk_state, ImDrawData* draw_data)
    {
        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        VkSemaphore image_acquired
            = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete
            = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

        VkResult err = vkAcquireNextImageKHR(vk_state.device, wd->Swapchain,
                                             UINT64_MAX, image_acquired,
                                             VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            vk_state.swapchain_rebuild = true;
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
            return;
        if (err != VK_SUBOPTIMAL_KHR)
            check_vk_result(err);

        ImGui_ImplVulkanH_Frame* frame = &wd->Frames[wd->FrameIndex];
        err = vkWaitForFences(vk_state.device, 1, &frame->Fence, VK_TRUE,
                              UINT64_MAX);
        check_vk_result(err);
        err = vkResetFences(vk_state.device, 1, &frame->Fence);
        check_vk_result(err);

        err = vkResetCommandPool(vk_state.device, frame->CommandPool, 0);
        check_vk_result(err);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(frame->CommandBuffer, &begin_info);
        check_vk_result(err);

        VkRenderPassBeginInfo rp_begin = {};
        rp_begin.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass  = wd->RenderPass;
        rp_begin.framebuffer = frame->Framebuffer;
        rp_begin.renderArea.extent.width  = wd->Width;
        rp_begin.renderArea.extent.height = wd->Height;
        rp_begin.clearValueCount          = 1;
        rp_begin.pClearValues             = &wd->ClearValue;
        vkCmdBeginRenderPass(frame->CommandBuffer, &rp_begin,
                             VK_SUBPASS_CONTENTS_INLINE);

        apply_imgui_texture_update_workarounds(vk_state, draw_data);
        ImGui_ImplVulkan_RenderDrawData(draw_data, frame->CommandBuffer);

        vkCmdEndRenderPass(frame->CommandBuffer);
        err = vkEndCommandBuffer(frame->CommandBuffer);
        check_vk_result(err);

        VkPipelineStageFlags wait_stage
            = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info         = {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &image_acquired;
        submit_info.pWaitDstStageMask    = &wait_stage;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &frame->CommandBuffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &render_complete;
        err = vkQueueSubmit(vk_state.queue, 1, &submit_info, frame->Fence);
        check_vk_result(err);
    }



    void frame_present(VulkanState& vk_state)
    {
        if (vk_state.swapchain_rebuild)
            return;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        VkSemaphore render_complete
            = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR present_info   = {};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &render_complete;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &wd->Swapchain;
        present_info.pImageIndices      = &wd->FrameIndex;
        VkResult err = vkQueuePresentKHR(vk_state.queue, &present_info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            vk_state.swapchain_rebuild = true;
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
            return;
        if (err != VK_SUBOPTIMAL_KHR)
            check_vk_result(err);

        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
    }



    void destroy_texture(VulkanState& vk_state, VulkanTexture& texture)
    {
        if (texture.set != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(texture.set);
            texture.set = VK_NULL_HANDLE;
        }
        if (texture.preview_source_set != VK_NULL_HANDLE
            && vk_state.preview_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(vk_state.device, vk_state.preview_descriptor_pool,
                                 1, &texture.preview_source_set);
            texture.preview_source_set = VK_NULL_HANDLE;
        }
        if (texture.preview_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vk_state.device, texture.preview_framebuffer,
                                 vk_state.allocator);
            texture.preview_framebuffer = VK_NULL_HANDLE;
        }
        if (texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.sampler,
                             vk_state.allocator);
            texture.sampler = VK_NULL_HANDLE;
        }
        if (texture.view != VK_NULL_HANDLE) {
            vkDestroyImageView(vk_state.device, texture.view,
                               vk_state.allocator);
            texture.view = VK_NULL_HANDLE;
        }
        if (texture.image != VK_NULL_HANDLE) {
            vkDestroyImage(vk_state.device, texture.image, vk_state.allocator);
            texture.image = VK_NULL_HANDLE;
        }
        if (texture.memory != VK_NULL_HANDLE) {
            vkFreeMemory(vk_state.device, texture.memory, vk_state.allocator);
            texture.memory = VK_NULL_HANDLE;
        }
        if (texture.source_view != VK_NULL_HANDLE) {
            vkDestroyImageView(vk_state.device, texture.source_view,
                               vk_state.allocator);
            texture.source_view = VK_NULL_HANDLE;
        }
        if (texture.source_image != VK_NULL_HANDLE) {
            vkDestroyImage(vk_state.device, texture.source_image,
                           vk_state.allocator);
            texture.source_image = VK_NULL_HANDLE;
        }
        if (texture.source_memory != VK_NULL_HANDLE) {
            vkFreeMemory(vk_state.device, texture.source_memory,
                         vk_state.allocator);
            texture.source_memory = VK_NULL_HANDLE;
        }
        texture.width  = 0;
        texture.height = 0;
        texture.preview_initialized = false;
        texture.preview_dirty = false;
        texture.preview_params_valid = false;
    }



    bool create_texture(VulkanState& vk_state, const LoadedImage& image,
                        VulkanTexture& texture, std::string& error_message)
    {
        destroy_texture(vk_state, texture);

        if (!vk_state.compute_upload_ready) {
            error_message = "compute upload path is not initialized";
            return false;
        }
        if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
            error_message = "invalid image data for texture upload";
            return false;
        }

        UploadDataType upload_type = image.type;
        size_t channel_bytes       = image.channel_bytes;
        size_t row_pitch_bytes     = image.row_pitch_bytes;
        const int channel_count    = std::max(0, image.nchannels);
        if (channel_count <= 0) {
            error_message = "invalid source channel count";
            return false;
        }

        std::vector<unsigned char> converted_pixels;
        const unsigned char* upload_ptr = image.pixels.data();
        size_t upload_bytes             = image.pixels.size();
        bool use_fp64_pipeline          = false;

        if (upload_type == UploadDataType::Double) {
            if (vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE) {
                use_fp64_pipeline = true;
                print("imiv: using fp64 compute upload path for '{}'\n",
                      image.path);
            } else {
                const size_t value_count = image.pixels.size() / sizeof(double);
                converted_pixels.resize(value_count * sizeof(float));
                const double* src = reinterpret_cast<const double*>(
                    image.pixels.data());
                float* dst = reinterpret_cast<float*>(converted_pixels.data());
                for (size_t i = 0; i < value_count; ++i)
                    dst[i] = static_cast<float>(src[i]);
                upload_type      = UploadDataType::Float;
                channel_bytes    = sizeof(float);
                row_pitch_bytes  = static_cast<size_t>(image.width)
                                  * static_cast<size_t>(channel_count)
                                  * channel_bytes;
                upload_ptr       = converted_pixels.data();
                upload_bytes     = converted_pixels.size();
                print(stderr,
                      "imiv: fp64 compute pipeline unavailable; converting "
                      "double input to float on CPU\n");
            }
        }

        const size_t pixel_stride_bytes
            = channel_bytes * static_cast<size_t>(channel_count);
        if (pixel_stride_bytes == 0 || row_pitch_bytes == 0
            || row_pitch_bytes < static_cast<size_t>(image.width)
                                    * pixel_stride_bytes) {
            error_message = "invalid source stride for compute upload";
            return false;
        }

        const VkDeviceSize upload_size_aligned
            = static_cast<VkDeviceSize>((upload_bytes + 3u) & ~size_t(3));

        VkBuffer staging_buffer           = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory     = VK_NULL_HANDLE;
        VkBuffer source_buffer            = VK_NULL_HANDLE;
        VkDeviceMemory source_memory      = VK_NULL_HANDLE;
        VkDescriptorSet compute_set       = VK_NULL_HANDLE;
        VkCommandPool upload_command_pool = VK_NULL_HANDLE;
        VkCommandBuffer upload_command    = VK_NULL_HANDLE;
        bool ok                           = false;

        do {
            VkResult err = VK_SUCCESS;

            VkImageCreateInfo source_ci = {};
            source_ci.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            source_ci.imageType         = VK_IMAGE_TYPE_2D;
            source_ci.format            = vk_state.compute_output_format;
            source_ci.extent.width      = static_cast<uint32_t>(image.width);
            source_ci.extent.height     = static_cast<uint32_t>(image.height);
            source_ci.extent.depth      = 1;
            source_ci.mipLevels         = 1;
            source_ci.arrayLayers       = 1;
            source_ci.samples           = VK_SAMPLE_COUNT_1_BIT;
            source_ci.tiling            = VK_IMAGE_TILING_OPTIMAL;
            source_ci.usage             = VK_IMAGE_USAGE_STORAGE_BIT
                              | VK_IMAGE_USAGE_SAMPLED_BIT;
            source_ci.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
            source_ci.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

            err = vkCreateImage(vk_state.device, &source_ci, vk_state.allocator,
                                &texture.source_image);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImage failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE,
                               texture.source_image, "imiv.viewer.source_image");

            VkMemoryRequirements source_reqs = {};
            vkGetImageMemoryRequirements(vk_state.device, texture.source_image,
                                         &source_reqs);

            uint32_t image_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  source_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  image_memory_type)) {
                error_message = "no device-local memory type for source image";
                break;
            }

            VkMemoryAllocateInfo source_alloc = {};
            source_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            source_alloc.allocationSize = source_reqs.size;
            source_alloc.memoryTypeIndex = image_memory_type;
            err = vkAllocateMemory(vk_state.device, &source_alloc,
                                   vk_state.allocator, &texture.source_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               texture.source_memory,
                               "imiv.viewer.source_image.memory");
            err = vkBindImageMemory(vk_state.device, texture.source_image,
                                    texture.source_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindImageMemory failed for source image";
                break;
            }

            VkImageViewCreateInfo source_view_ci = {};
            source_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            source_view_ci.image = texture.source_image;
            source_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            source_view_ci.format = vk_state.compute_output_format;
            source_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            source_view_ci.subresourceRange.baseMipLevel = 0;
            source_view_ci.subresourceRange.levelCount = 1;
            source_view_ci.subresourceRange.baseArrayLayer = 0;
            source_view_ci.subresourceRange.layerCount = 1;
            err = vkCreateImageView(vk_state.device, &source_view_ci,
                                    vk_state.allocator, &texture.source_view);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImageView failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW,
                               texture.source_view, "imiv.viewer.source_view");

            VkImageCreateInfo preview_ci = source_ci;
            preview_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT;
            err = vkCreateImage(vk_state.device, &preview_ci, vk_state.allocator,
                                &texture.image);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImage failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE, texture.image,
                               "imiv.viewer.preview_image");

            VkMemoryRequirements preview_reqs = {};
            vkGetImageMemoryRequirements(vk_state.device, texture.image,
                                         &preview_reqs);
            uint32_t preview_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  preview_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  preview_memory_type)) {
                error_message = "no device-local memory type for preview image";
                break;
            }
            VkMemoryAllocateInfo preview_alloc = {};
            preview_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            preview_alloc.allocationSize = preview_reqs.size;
            preview_alloc.memoryTypeIndex = preview_memory_type;
            err = vkAllocateMemory(vk_state.device, &preview_alloc,
                                   vk_state.allocator, &texture.memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               texture.memory, "imiv.viewer.preview_image.memory");
            err = vkBindImageMemory(vk_state.device, texture.image, texture.memory,
                                    0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindImageMemory failed for preview image";
                break;
            }

            VkImageViewCreateInfo preview_view_ci = source_view_ci;
            preview_view_ci.image = texture.image;
            err = vkCreateImageView(vk_state.device, &preview_view_ci,
                                    vk_state.allocator, &texture.view);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImageView failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW, texture.view,
                               "imiv.viewer.preview_view");

            VkFramebufferCreateInfo fb_ci = {};
            fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_ci.renderPass = vk_state.preview_render_pass;
            fb_ci.attachmentCount = 1;
            fb_ci.pAttachments = &texture.view;
            fb_ci.width = static_cast<uint32_t>(image.width);
            fb_ci.height = static_cast<uint32_t>(image.height);
            fb_ci.layers = 1;
            err = vkCreateFramebuffer(vk_state.device, &fb_ci,
                                      vk_state.allocator,
                                      &texture.preview_framebuffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateFramebuffer failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_FRAMEBUFFER,
                               texture.preview_framebuffer,
                               "imiv.viewer.preview_framebuffer");

            VkBufferCreateInfo buffer_ci = {};
            buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size = upload_size_aligned;
            buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                              | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            buffer_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &buffer_ci,
                                 vk_state.allocator, &source_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateBuffer failed for source buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, source_buffer,
                               "imiv.viewer.upload.source_buffer");

            VkMemoryRequirements staging_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, source_buffer,
                                          &staging_reqs);

            uint32_t staging_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  staging_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  staging_memory_type)) {
                error_message = "no device-local memory type for source buffer";
                break;
            }

            VkMemoryAllocateInfo staging_alloc = {};
            staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            staging_alloc.allocationSize  = staging_reqs.size;
            staging_alloc.memoryTypeIndex = staging_memory_type;
            err = vkAllocateMemory(vk_state.device, &staging_alloc,
                                   vk_state.allocator, &source_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for source buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               source_memory, "imiv.viewer.upload.source_memory");
            err = vkBindBufferMemory(vk_state.device, source_buffer,
                                     source_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindBufferMemory failed for source buffer";
                break;
            }

            VkBufferCreateInfo staging_ci = {};
            staging_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_ci.size               = upload_size_aligned;
            staging_ci.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staging_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &staging_ci,
                                 vk_state.allocator, &staging_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateBuffer failed for staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                               "imiv.viewer.upload.staging_buffer");

            VkMemoryRequirements staging_buffer_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                          &staging_buffer_reqs);
            uint32_t host_visible_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  staging_buffer_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  host_visible_memory_type)) {
                error_message = "no host-visible memory type for staging buffer";
                break;
            }
            VkMemoryAllocateInfo host_alloc = {};
            host_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            host_alloc.allocationSize  = staging_buffer_reqs.size;
            host_alloc.memoryTypeIndex = host_visible_memory_type;
            err = vkAllocateMemory(vk_state.device, &host_alloc,
                                   vk_state.allocator, &staging_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               staging_memory,
                               "imiv.viewer.upload.staging_memory");
            err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                     staging_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindBufferMemory failed for staging buffer";
                break;
            }

            void* mapped = nullptr;
            err = vkMapMemory(vk_state.device, staging_memory, 0,
                              upload_size_aligned,
                              0, &mapped);
            if (err != VK_SUCCESS || mapped == nullptr) {
                error_message = "vkMapMemory failed for staging buffer";
                break;
            }
            std::memset(mapped, 0, static_cast<size_t>(upload_size_aligned));
            std::memcpy(mapped, upload_ptr, upload_bytes);
            vkUnmapMemory(vk_state.device, staging_memory);

            VkDescriptorSetAllocateInfo set_alloc = {};
            set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            set_alloc.descriptorPool = vk_state.compute_descriptor_pool;
            set_alloc.descriptorSetCount = 1;
            set_alloc.pSetLayouts = &vk_state.compute_descriptor_set_layout;
            err = vkAllocateDescriptorSets(vk_state.device, &set_alloc,
                                           &compute_set);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateDescriptorSets failed for upload compute";
                break;
            }

            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                      vk_state.allocator, &upload_command_pool);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateCommandPool failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                               upload_command_pool,
                               "imiv.viewer.upload.command_pool");

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool        = upload_command_pool;
            command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &upload_command);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateCommandBuffers failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               upload_command,
                               "imiv.viewer.upload.command_buffer");

            VkDescriptorBufferInfo source_buffer_info = {};
            source_buffer_info.buffer = source_buffer;
            source_buffer_info.offset = 0;
            source_buffer_info.range  = upload_size_aligned;

            VkDescriptorImageInfo output_image_info = {};
            output_image_info.imageView   = texture.source_view;
            output_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            output_image_info.sampler     = VK_NULL_HANDLE;

            VkWriteDescriptorSet writes[2] = {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = compute_set;
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[0].pBufferInfo = &source_buffer_info;
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = compute_set;
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &output_image_info;
            vkUpdateDescriptorSets(vk_state.device, 2, writes, 0, nullptr);

            VkSamplerCreateInfo sampler_ci = {};
            sampler_ci.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            VkFormatProperties output_props = {};
            vkGetPhysicalDeviceFormatProperties(vk_state.physical_device,
                                                vk_state.compute_output_format,
                                                &output_props);
            const bool has_linear_filter
                = (output_props.optimalTilingFeatures
                   & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
                  != 0;
            sampler_ci.magFilter = has_linear_filter ? VK_FILTER_LINEAR
                                                     : VK_FILTER_NEAREST;
            sampler_ci.minFilter = has_linear_filter ? VK_FILTER_LINEAR
                                                     : VK_FILTER_NEAREST;
            sampler_ci.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_ci.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.minLod        = -1000.0f;
            sampler_ci.maxLod        = 1000.0f;
            sampler_ci.maxAnisotropy = 1.0f;
            err = vkCreateSampler(vk_state.device, &sampler_ci,
                                  vk_state.allocator, &texture.sampler);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateSampler failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                               texture.sampler, "imiv.viewer.sampler");

            VkDescriptorSetAllocateInfo preview_set_alloc = {};
            preview_set_alloc.sType
                = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            preview_set_alloc.descriptorPool = vk_state.preview_descriptor_pool;
            preview_set_alloc.descriptorSetCount = 1;
            preview_set_alloc.pSetLayouts
                = &vk_state.preview_descriptor_set_layout;
            err = vkAllocateDescriptorSets(vk_state.device, &preview_set_alloc,
                                           &texture.preview_source_set);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateDescriptorSets failed for preview source set";
                break;
            }
            VkDescriptorImageInfo preview_source_image = {};
            preview_source_image.sampler = texture.sampler;
            preview_source_image.imageView = texture.source_view;
            preview_source_image.imageLayout
                = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet preview_write = {};
            preview_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            preview_write.dstSet = texture.preview_source_set;
            preview_write.dstBinding = 0;
            preview_write.descriptorCount = 1;
            preview_write.descriptorType
                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            preview_write.pImageInfo = &preview_source_image;
            vkUpdateDescriptorSets(vk_state.device, 1, &preview_write, 0,
                                   nullptr);

            VkCommandBufferBeginInfo command_begin = {};
            command_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            command_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(upload_command, &command_begin);
            if (err != VK_SUCCESS) {
                error_message = "vkBeginCommandBuffer failed";
                break;
            }

            VkBufferCopy copy_region = {};
            copy_region.srcOffset = 0;
            copy_region.dstOffset = 0;
            copy_region.size      = upload_size_aligned;
            vkCmdCopyBuffer(upload_command, staging_buffer, source_buffer, 1,
                            &copy_region);

            VkBufferMemoryBarrier source_to_compute = {};
            source_to_compute.sType
                = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            source_to_compute.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            source_to_compute.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            source_to_compute.srcQueueFamilyIndex
                = VK_QUEUE_FAMILY_IGNORED;
            source_to_compute.dstQueueFamilyIndex
                = VK_QUEUE_FAMILY_IGNORED;
            source_to_compute.buffer = source_buffer;
            source_to_compute.offset = 0;
            source_to_compute.size   = upload_size_aligned;

            VkImageMemoryBarrier image_to_general = {};
            image_to_general.sType
                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_to_general.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_to_general.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_to_general.srcQueueFamilyIndex
                = VK_QUEUE_FAMILY_IGNORED;
            image_to_general.dstQueueFamilyIndex
                = VK_QUEUE_FAMILY_IGNORED;
            image_to_general.image = texture.source_image;
            image_to_general.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            image_to_general.subresourceRange.baseMipLevel = 0;
            image_to_general.subresourceRange.levelCount   = 1;
            image_to_general.subresourceRange.baseArrayLayer = 0;
            image_to_general.subresourceRange.layerCount     = 1;
            image_to_general.srcAccessMask = 0;
            image_to_general.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(upload_command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                                 nullptr, 1, &source_to_compute, 1,
                                 &image_to_general);

            vkCmdBindPipeline(upload_command, VK_PIPELINE_BIND_POINT_COMPUTE,
                              use_fp64_pipeline
                                  ? vk_state.compute_pipeline_fp64
                                  : vk_state.compute_pipeline);
            vkCmdBindDescriptorSets(upload_command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    vk_state.compute_pipeline_layout, 0, 1,
                                    &compute_set, 0, nullptr);

            UploadComputePushConstants push = {};
            push.width          = static_cast<uint32_t>(image.width);
            push.height         = static_cast<uint32_t>(image.height);
            push.row_pitch_bytes = static_cast<uint32_t>(row_pitch_bytes);
            push.pixel_stride    = static_cast<uint32_t>(pixel_stride_bytes);
            push.channel_count   = static_cast<uint32_t>(channel_count);
            push.data_type       = static_cast<uint32_t>(upload_type);
            vkCmdPushConstants(upload_command, vk_state.compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               &push);

            const uint32_t group_x
                = (push.width + 15u) / 16u;
            const uint32_t group_y
                = (push.height + 15u) / 16u;
            vkCmdDispatch(upload_command, group_x, group_y, 1);

            VkImageMemoryBarrier to_shader = {};
            to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader.image = texture.source_image;
            to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader.subresourceRange.baseMipLevel = 0;
            to_shader.subresourceRange.levelCount = 1;
            to_shader.subresourceRange.baseArrayLayer = 0;
            to_shader.subresourceRange.layerCount = 1;
            to_shader.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(upload_command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_shader);

            err = vkEndCommandBuffer(upload_command);
            if (err != VK_SUCCESS) {
                error_message = "vkEndCommandBuffer failed";
                break;
            }

            VkSubmitInfo submit_info       = {};
            submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers    = &upload_command;
            err = vkQueueSubmit(vk_state.queue, 1, &submit_info,
                                VK_NULL_HANDLE);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueSubmit failed";
                break;
            }
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueWaitIdle failed";
                break;
            }

            texture.set = ImGui_ImplVulkan_AddTexture(
                texture.sampler, texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (texture.set == VK_NULL_HANDLE) {
                error_message = "ImGui_ImplVulkan_AddTexture failed";
                break;
            }

            texture.width  = image.width;
            texture.height = image.height;
            texture.preview_initialized = false;
            texture.preview_dirty       = true;
            texture.preview_params_valid = false;
            ok             = true;

            if (compute_set != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(vk_state.device,
                                     vk_state.compute_descriptor_pool, 1,
                                     &compute_set);
                compute_set = VK_NULL_HANDLE;
            }
        } while (false);

        if (compute_set != VK_NULL_HANDLE)
            vkFreeDescriptorSets(vk_state.device,
                                 vk_state.compute_descriptor_pool, 1,
                                 &compute_set);
        if (upload_command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, upload_command_pool,
                                 vk_state.allocator);
        if (source_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, source_buffer,
                            vk_state.allocator);
        if (source_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, source_memory, vk_state.allocator);
        if (staging_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
        if (!ok)
            destroy_texture(vk_state, texture);
        return ok;
    }



    bool preview_controls_equal(const PreviewControls& a,
                                const PreviewControls& b)
    {
        return std::abs(a.exposure - b.exposure) < 1.0e-6f
               && std::abs(a.gamma - b.gamma) < 1.0e-6f
               && a.color_mode == b.color_mode && a.channel == b.channel
               && a.use_ocio == b.use_ocio;
    }



    bool update_preview_texture(VulkanState& vk_state, VulkanTexture& texture,
                                const PreviewControls& controls,
                                std::string& error_message)
    {
        if (texture.image == VK_NULL_HANDLE || texture.source_image == VK_NULL_HANDLE
            || texture.preview_framebuffer == VK_NULL_HANDLE
            || texture.preview_source_set == VK_NULL_HANDLE)
            return false;

        if (texture.preview_params_valid
            && preview_controls_equal(texture.last_preview_controls, controls)
            && !texture.preview_dirty) {
            return true;
        }

        VkCommandPool command_pool = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        bool ok = false;

        do {
            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            VkResult err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                               vk_state.allocator, &command_pool);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateCommandPool failed for preview update";
                break;
            }

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType
                = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool = command_pool;
            command_alloc.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &command_buffer);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateCommandBuffers failed for preview update";
                break;
            }

            VkCommandBufferBeginInfo begin = {};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(command_buffer, &begin);
            if (err != VK_SUCCESS) {
                error_message = "vkBeginCommandBuffer failed for preview update";
                break;
            }

            VkImageMemoryBarrier to_color_attachment = {};
            to_color_attachment.sType
                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_color_attachment.oldLayout
                = texture.preview_initialized
                      ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                      : VK_IMAGE_LAYOUT_UNDEFINED;
            to_color_attachment.newLayout
                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_color_attachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color_attachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color_attachment.image = texture.image;
            to_color_attachment.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            to_color_attachment.subresourceRange.baseMipLevel = 0;
            to_color_attachment.subresourceRange.levelCount   = 1;
            to_color_attachment.subresourceRange.baseArrayLayer = 0;
            to_color_attachment.subresourceRange.layerCount     = 1;
            to_color_attachment.srcAccessMask = texture.preview_initialized
                                                    ? VK_ACCESS_SHADER_READ_BIT
                                                    : 0;
            to_color_attachment.dstAccessMask
                = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command_buffer,
                                 texture.preview_initialized
                                     ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                     : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0, 0, nullptr, 0, nullptr, 1,
                                 &to_color_attachment);

            VkClearValue clear = {};
            clear.color.float32[0] = 0.0f;
            clear.color.float32[1] = 0.0f;
            clear.color.float32[2] = 0.0f;
            clear.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.renderPass = vk_state.preview_render_pass;
            rp_begin.framebuffer = texture.preview_framebuffer;
            rp_begin.renderArea.offset = { 0, 0 };
            rp_begin.renderArea.extent.width = static_cast<uint32_t>(texture.width);
            rp_begin.renderArea.extent.height
                = static_cast<uint32_t>(texture.height);
            rp_begin.clearValueCount = 1;
            rp_begin.pClearValues    = &clear;

            vkCmdBeginRenderPass(command_buffer, &rp_begin,
                                 VK_SUBPASS_CONTENTS_INLINE);
            VkViewport vp = {};
            vp.x        = 0.0f;
            vp.y        = 0.0f;
            vp.width    = static_cast<float>(texture.width);
            vp.height   = static_cast<float>(texture.height);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            VkRect2D scissor = {};
            scissor.extent.width = static_cast<uint32_t>(texture.width);
            scissor.extent.height = static_cast<uint32_t>(texture.height);
            vkCmdSetViewport(command_buffer, 0, 1, &vp);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              vk_state.preview_pipeline);
            vkCmdBindDescriptorSets(command_buffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_state.preview_pipeline_layout, 0, 1,
                                    &texture.preview_source_set, 0, nullptr);

            PreviewPushConstants push = {};
            push.exposure   = controls.exposure;
            push.gamma      = std::max(0.01f, controls.gamma);
            push.color_mode = controls.color_mode;
            push.channel    = controls.channel;
            push.use_ocio   = controls.use_ocio;
            vkCmdPushConstants(command_buffer, vk_state.preview_pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                               &push);
            vkCmdDraw(command_buffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(command_buffer);

            VkImageMemoryBarrier to_shader_read = {};
            to_shader_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader_read.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.image = texture.image;
            to_shader_read.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader_read.subresourceRange.baseMipLevel = 0;
            to_shader_read.subresourceRange.levelCount   = 1;
            to_shader_read.subresourceRange.baseArrayLayer = 0;
            to_shader_read.subresourceRange.layerCount     = 1;
            to_shader_read.srcAccessMask
                = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_shader_read);

            err = vkEndCommandBuffer(command_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkEndCommandBuffer failed for preview update";
                break;
            }

            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers    = &command_buffer;
            err = vkQueueSubmit(vk_state.queue, 1, &submit, VK_NULL_HANDLE);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueSubmit failed for preview update";
                break;
            }
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueWaitIdle failed for preview update";
                break;
            }

            texture.preview_initialized = true;
            texture.preview_dirty       = false;
            texture.preview_params_valid = true;
            texture.last_preview_controls = controls;
            ok = true;
        } while (false);

        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, command_pool,
                                 vk_state.allocator);
        return ok;
    }



    bool load_viewer_image(VulkanState& vk_state, ViewerState& viewer,
                           const std::string& path)
    {
        viewer.last_error.clear();
        LoadedImage loaded;
        std::string error;
        if (!load_image_for_compute(path, loaded, error)) {
            viewer.last_error = Strutil::fmt::format("open failed: {}", error);
            return false;
        }
        VulkanTexture texture;
        if (!create_texture(vk_state, loaded, texture, error)) {
            viewer.last_error = Strutil::fmt::format("upload failed: {}",
                                                     error);
            return false;
        }
        destroy_texture(vk_state, viewer.texture);
        if (!viewer.image.path.empty())
            viewer.toggle_image_path = viewer.image.path;
        viewer.image       = std::move(loaded);
        viewer.texture     = texture;
        viewer.zoom        = 1.0f;
        viewer.fit_request = true;
        refresh_sibling_images(viewer);
        viewer.status_message
            = Strutil::fmt::format("Loaded {} ({}x{}, {} channels, {})",
                                   viewer.image.path, viewer.image.width,
                                   viewer.image.height, viewer.image.nchannels,
                                   upload_data_type_name(viewer.image.type));
        return true;
    }

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    TestEngineRuntime* g_imiv_test_runtime = nullptr;



    bool capture_swapchain_region_rgba8(VulkanState& vk_state, int x, int y,
                                        int w, int h, unsigned int* pixels)
    {
        if (pixels == nullptr || w <= 0 || h <= 0)
            return false;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        if (wd->FrameIndex >= static_cast<uint32_t>(wd->Frames.Size))
            return false;
        if (x < 0 || y < 0 || x + w > wd->Width || y + h > wd->Height)
            return false;

        VkImage image = wd->Frames[wd->FrameIndex].Backbuffer;
        if (image == VK_NULL_HANDLE)
            return false;

        const int full_width  = wd->Width;
        const int full_height = wd->Height;
        if (full_width <= 0 || full_height <= 0)
            return false;

        VkResult err = vkQueueWaitIdle(vk_state.queue);
        if (err != VK_SUCCESS)
            return false;

        VkBuffer staging_buffer       = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
        VkCommandPool command_pool    = VK_NULL_HANDLE;
        VkCommandBuffer command_buf   = VK_NULL_HANDLE;
        const VkDeviceSize full_buffer_size
            = static_cast<VkDeviceSize>(full_width)
              * static_cast<VkDeviceSize>(full_height) * 4;
        bool ok = false;

        do {
            VkBufferCreateInfo buffer_ci = {};
            buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size               = full_buffer_size;
            buffer_ci.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &buffer_ci,
                                 vk_state.allocator, &staging_buffer);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                               "imiv.capture.readback.staging_buffer");

            VkMemoryRequirements memory_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                          &memory_reqs);

            uint32_t memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  memory_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  memory_type))
                break;

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize  = memory_reqs.size;
            alloc_info.memoryTypeIndex = memory_type;
            err = vkAllocateMemory(vk_state.device, &alloc_info,
                                   vk_state.allocator, &staging_memory);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               staging_memory,
                               "imiv.capture.readback.staging_memory");

            err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                     staging_memory, 0);
            if (err != VK_SUCCESS)
                break;

            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                      vk_state.allocator, &command_pool);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                               command_pool, "imiv.capture.command_pool");

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool        = command_pool;
            command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &command_buf);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               command_buf, "imiv.capture.command_buffer");

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err              = vkBeginCommandBuffer(command_buf, &begin_info);
            if (err != VK_SUCCESS)
                break;

            VkImageMemoryBarrier to_transfer = {};
            to_transfer.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            to_transfer.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.image                       = image;
            to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_transfer.subresourceRange.baseMipLevel   = 0;
            to_transfer.subresourceRange.levelCount     = 1;
            to_transfer.subresourceRange.baseArrayLayer = 0;
            to_transfer.subresourceRange.layerCount     = 1;
            to_transfer.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(command_buf,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &to_transfer);

            VkBufferImageCopy copy_region           = {};
            copy_region.bufferOffset                = 0;
            copy_region.bufferRowLength             = 0;
            copy_region.bufferImageHeight           = 0;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel   = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount     = 1;
            copy_region.imageOffset.x                   = 0;
            copy_region.imageOffset.y                   = 0;
            copy_region.imageOffset.z                   = 0;
            copy_region.imageExtent.width  = static_cast<uint32_t>(full_width);
            copy_region.imageExtent.height = static_cast<uint32_t>(full_height);
            copy_region.imageExtent.depth  = 1;
            vkCmdCopyImageToBuffer(command_buf, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   staging_buffer, 1, &copy_region);

            VkImageMemoryBarrier to_present = {};
            to_present.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            to_present.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_present.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_present.image                       = image;
            to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_present.subresourceRange.baseMipLevel   = 0;
            to_present.subresourceRange.levelCount     = 1;
            to_present.subresourceRange.baseArrayLayer = 0;
            to_present.subresourceRange.layerCount     = 1;
            to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            to_present.dstAccessMask = 0;
            vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_present);

            err = vkEndCommandBuffer(command_buf);
            if (err != VK_SUCCESS)
                break;

            VkSubmitInfo submit_info       = {};
            submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers    = &command_buf;
            err = vkQueueSubmit(vk_state.queue, 1, &submit_info,
                                VK_NULL_HANDLE);
            if (err != VK_SUCCESS)
                break;
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS)
                break;

            void* mapped = nullptr;
            err          = vkMapMemory(vk_state.device, staging_memory, 0,
                                       full_buffer_size, 0, &mapped);
            if (err != VK_SUCCESS || mapped == nullptr)
                break;

            const unsigned char* src = reinterpret_cast<const unsigned char*>(
                mapped);
            unsigned char* dst = reinterpret_cast<unsigned char*>(pixels);
            const size_t src_row_bytes = static_cast<size_t>(full_width) * 4;
            const size_t dst_row_bytes = static_cast<size_t>(w) * 4;
            for (int row = 0; row < h; ++row) {
                const size_t src_offset = (static_cast<size_t>(y + row)
                                           * src_row_bytes)
                                          + static_cast<size_t>(x) * 4;
                std::memcpy(dst + static_cast<size_t>(row) * dst_row_bytes,
                            src + src_offset, dst_row_bytes);
            }
            vkUnmapMemory(vk_state.device, staging_memory);

            VkFormat format        = wd->SurfaceFormat.format;
            const bool bgra_source = (format == VK_FORMAT_B8G8R8A8_UNORM
                                      || format == VK_FORMAT_B8G8R8A8_SRGB);
            if (bgra_source) {
                unsigned char* bytes = reinterpret_cast<unsigned char*>(pixels);
                const size_t pixel_count = static_cast<size_t>(w)
                                           * static_cast<size_t>(h);
                for (size_t i = 0; i < pixel_count; ++i) {
                    unsigned char* px = bytes + i * 4;
                    std::swap(px[0], px[2]);
                }
            }
            ok = true;
        } while (false);

        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, command_pool,
                                 vk_state.allocator);
        if (staging_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
        return ok;
    }



    bool imiv_vulkan_screen_capture(ImGuiID viewport_id, int x, int y, int w,
                                    int h, unsigned int* pixels,
                                    void* user_data)
    {
        (void)viewport_id;
        VulkanState* vk_state = reinterpret_cast<VulkanState*>(user_data);
        if (vk_state == nullptr)
            return false;
        return capture_swapchain_region_rgba8(*vk_state, x, y, w, h, pixels);
    }



    void mark_test_error(ImGuiTestContext* ctx)
    {
        if (ctx && ctx->TestOutput
            && ctx->TestOutput->Status == ImGuiTestStatus_Running) {
            ctx->TestOutput->Status = ImGuiTestStatus_Error;
        }
    }



    void json_write_escaped(FILE* f, const char* s)
    {
        std::fputc('"', f);
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(
                 s ? s : "");
             *p; ++p) {
            unsigned char c = *p;
            switch (c) {
            case '\\': std::fputs("\\\\", f); break;
            case '"': std::fputs("\\\"", f); break;
            case '\b': std::fputs("\\b", f); break;
            case '\f': std::fputs("\\f", f); break;
            case '\n': std::fputs("\\n", f); break;
            case '\r': std::fputs("\\r", f); break;
            case '\t': std::fputs("\\t", f); break;
            default:
                if (c < 0x20)
                    std::fprintf(f, "\\u%04x", static_cast<unsigned int>(c));
                else
                    std::fputc(static_cast<int>(c), f);
                break;
            }
        }
        std::fputc('"', f);
    }



    void json_write_vec2(FILE* f, const ImVec2& v)
    {
        std::fprintf(f, "[%.3f,%.3f]", static_cast<double>(v.x),
                     static_cast<double>(v.y));
    }



    void json_write_rect(FILE* f, const ImRect& r)
    {
        std::fputs("{\"min\":", f);
        json_write_vec2(f, r.Min);
        std::fputs(",\"max\":", f);
        json_write_vec2(f, r.Max);
        std::fputs("}", f);
    }



    bool write_layout_dump_json(ImGuiTestContext* ctx,
                                const std::filesystem::path& out_path,
                                bool include_items, int depth)
    {
        if (depth <= 0)
            depth = 1;

        std::error_code ec;
        if (!out_path.parent_path().empty())
            std::filesystem::create_directories(out_path.parent_path(), ec);

        FILE* f = nullptr;
#        if defined(_WIN32)
        if (fopen_s(&f, out_path.string().c_str(), "wb") != 0)
            f = nullptr;
#        else
        f = std::fopen(out_path.string().c_str(), "wb");
#        endif
        if (!f) {
            ctx->LogError("layout dump: failed to open output file: %s",
                          out_path.string().c_str());
            mark_test_error(ctx);
            return false;
        }

        ImGuiIO& io           = ImGui::GetIO();
        ImGuiTestItemInfo win = ctx->WindowInfo(k_image_window_title,
                                                ImGuiTestOpFlags_NoError);
        if (win.ID == 0 || win.Window == nullptr) {
            std::fclose(f);
            ctx->LogError("layout dump: could not resolve window '%s'",
                          k_image_window_title);
            mark_test_error(ctx);
            return false;
        }

        std::fputs("{\n", f);
        std::fputs("  \"frame_count\": ", f);
        std::fprintf(f, "%d,\n", ImGui::GetFrameCount());
        std::fputs("  \"display_size\": ", f);
        json_write_vec2(f, io.DisplaySize);
        std::fputs(",\n", f);
        std::fputs("  \"windows\": [\n", f);
        std::fputs("    {\"name\": ", f);
        json_write_escaped(f, win.Window->Name);
        std::fputs(", \"id\": ", f);
        std::fprintf(f, "%u", static_cast<unsigned int>(win.Window->ID));
        std::fputs(", \"viewport_id\": ", f);
        std::fprintf(f, "%u",
                     static_cast<unsigned int>(win.Window->ViewportId));
        std::fputs(", \"pos\": ", f);
        json_write_vec2(f, win.Window->Pos);
        std::fputs(", \"size\": ", f);
        json_write_vec2(f, win.Window->Size);
        std::fputs(", \"rect\": ", f);
        json_write_rect(f, win.Window->Rect());
        std::fputs(", \"collapsed\": ", f);
        std::fputs(win.Window->Collapsed ? "true" : "false", f);
        std::fputs(", \"active\": ", f);
        std::fputs(win.Window->Active ? "true" : "false", f);
        std::fputs(", \"was_active\": ", f);
        std::fputs(win.Window->WasActive ? "true" : "false", f);
        std::fputs(", \"hidden\": ", f);
        std::fputs(win.Window->Hidden ? "true" : "false", f);

        if (include_items) {
            std::fputs(", \"items\": [\n", f);
            ImGuiTestItemList list;
            list.Reserve(4096);
            ctx->GatherItems(&list, ImGuiTestRef(win.Window->ID), depth);

            int emitted_items = 0;
            for (int i = 0; i < list.GetSize(); ++i) {
                const ImGuiTestItemInfo* item = list.GetByIndex(i);
                if (item == nullptr || item->ID == 0 || item->Window == nullptr)
                    continue;
                if (emitted_items++ > 0)
                    std::fputs(",\n", f);
                std::fputs("      {\"id\": ", f);
                std::fprintf(f, "%u", static_cast<unsigned int>(item->ID));
                std::fputs(", \"parent_id\": ", f);
                std::fprintf(f, "%u",
                             static_cast<unsigned int>(item->ParentID));
                std::fputs(", \"depth\": ", f);
                std::fprintf(f, "%d", static_cast<int>(item->Depth));
                std::fputs(", \"debug\": ", f);
                json_write_escaped(f, item->DebugLabel);
                std::fputs(", \"rect_full\": ", f);
                json_write_rect(f, item->RectFull);
                std::fputs(", \"rect_clipped\": ", f);
                json_write_rect(f, item->RectClipped);
                std::fputs(", \"item_flags\": ", f);
                std::fprintf(f, "%u",
                             static_cast<unsigned int>(item->ItemFlags));
                std::fputs(", \"status_flags\": ", f);
                std::fprintf(f, "%u",
                             static_cast<unsigned int>(item->StatusFlags));
                std::fputs("}", f);
            }
            if (emitted_items > 0)
                std::fputs("\n", f);
            std::fputs("    ]", f);
        }

        std::fputs("}\n", f);
        std::fputs("  ]\n}\n", f);
        std::fflush(f);
        std::fclose(f);
        ctx->LogInfo("layout dump: wrote %s", out_path.string().c_str());
        return true;
    }



    void capture_main_viewport_screenshot(ImGuiTestContext* ctx,
                                          const char* out_file)
    {
        ctx->CaptureReset();
        if (out_file && out_file[0] != '\0') {
            std::snprintf(ctx->CaptureArgs->InOutputFile,
                          sizeof(ctx->CaptureArgs->InOutputFile), "%s",
                          out_file);
        }

        ImGuiViewport* vp                   = ImGui::GetMainViewport();
        ctx->CaptureArgs->InCaptureRect.Min = vp->Pos;
        ctx->CaptureArgs->InCaptureRect.Max = ImVec2(vp->Pos.x + vp->Size.x,
                                                     vp->Pos.y + vp->Size.y);
        ctx->CaptureScreenshot(ImGuiCaptureFlags_Instant
                               | ImGuiCaptureFlags_HideMouseCursor);
    }



    void ImivTest_SmokeScreenshot(ImGuiTestContext* ctx)
    {
        int delay_frames = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES", 3);
        ctx->Yield(delay_frames);

        int frames_to_capture
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES", 1);
        bool save_all = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_SAVE_ALL");

        std::string out_value;
        bool has_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT",
                             out_value)
              && !out_value.empty();
        const char* out = has_out ? out_value.c_str() : nullptr;

        bool capture_last_only = (!save_all && frames_to_capture > 1);
        for (int i = 0; i < frames_to_capture; ++i) {
            if (capture_last_only && i + 1 != frames_to_capture) {
                ctx->Yield(1);
                continue;
            }
            if (out) {
                if (save_all && frames_to_capture > 1) {
                    const char* dot = std::strrchr(out, '.');
                    if (dot && dot != out) {
                        size_t base_len     = static_cast<size_t>(dot - out);
                        char out_file[2048] = {};
                        std::snprintf(out_file, sizeof(out_file),
                                      "%.*s_f%03d%s",
                                      static_cast<int>(base_len), out, i, dot);
                        capture_main_viewport_screenshot(ctx, out_file);
                    } else {
                        char out_file[2048] = {};
                        std::snprintf(out_file, sizeof(out_file),
                                      "%s_f%03d.png", out, i);
                        capture_main_viewport_screenshot(ctx, out_file);
                    }
                } else {
                    capture_main_viewport_screenshot(ctx, out);
                }
            } else {
                capture_main_viewport_screenshot(ctx, nullptr);
            }
            ctx->Yield(1);
        }

        if (g_imiv_test_runtime != nullptr)
            g_imiv_test_runtime->request_exit = true;
    }



    void ImivTest_DumpLayoutJson(ImGuiTestContext* ctx)
    {
        int delay_frames
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DELAY_FRAMES",
                            3);
        ctx->Yield(delay_frames);

        bool include_items = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS");
        int depth = env_int_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DEPTH",
                                  3);
        if (depth <= 0)
            depth = 1;

        std::string out_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT", out_value)
            || out_value.empty()) {
            out_value = "layout.json";
        }

        if (!write_layout_dump_json(ctx, std::filesystem::path(out_value),
                                    include_items, depth)) {
            if (g_imiv_test_runtime != nullptr)
                g_imiv_test_runtime->request_exit = true;
            return;
        }
        if (g_imiv_test_runtime != nullptr)
            g_imiv_test_runtime->request_exit = true;
    }



    ImGuiTest* register_imiv_smoke_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "smoke_screenshot");
        t->TestFunc  = ImivTest_SmokeScreenshot;
        return t;
    }



    ImGuiTest* register_imiv_layout_dump_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "dump_layout_json");
        t->TestFunc  = ImivTest_DumpLayoutJson;
        return t;
    }



    TestEngineConfig gather_test_engine_config()
    {
        TestEngineConfig cfg;
        cfg.trace = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_TRACE");
        cfg.auto_screenshot
            = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
              || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTO_SCREENSHOT");

        std::string layout_out;
        const bool has_layout_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT",
                             layout_out)
              && !layout_out.empty();
        cfg.layout_dump = env_flag_is_truthy(
                              "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
                          || has_layout_out;

        std::string junit_out;
        const bool has_junit_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_JUNIT_OUT", junit_out)
              && !junit_out.empty();
        cfg.junit_xml = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML")
                        || has_junit_out;
        cfg.junit_xml_out = has_junit_out ? junit_out : "imiv_tests.junit.xml";

        cfg.want_test_engine = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
                               || cfg.auto_screenshot || cfg.layout_dump
                               || cfg.junit_xml;
#        if !defined(NDEBUG)
        cfg.want_test_engine = true;
#        endif
        cfg.automation_mode = cfg.auto_screenshot || cfg.layout_dump;
        cfg.exit_on_finish  = env_flag_is_truthy(
                                 "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH")
                             || cfg.automation_mode;
        cfg.show_windows = false;
        return cfg;
    }
#    endif
#endif



    ImGuiID begin_main_dockspace()
    {
        ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_None;
        return ImGui::DockSpaceOverViewport(ImGui::GetID("ImivMainDockspace"),
                                            ImGui::GetMainViewport(),
                                            dock_flags);
    }



    const char* channel_view_name(int mode)
    {
        switch (mode) {
        case 0: return "Full Color";
        case 1: return "Red";
        case 2: return "Green";
        case 3: return "Blue";
        case 4: return "Alpha";
        default: break;
        }
        return "Unknown";
    }



    const char* color_mode_name(int mode)
    {
        switch (mode) {
        case 0: return "RGBA";
        case 1: return "RGB";
        case 2: return "Single channel";
        case 3: return "Luminance";
        case 4: return "Heatmap";
        default: break;
        }
        return "Unknown";
    }



    bool has_supported_image_extension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), to_lower_ascii);
        return ext == ".exr" || ext == ".tif" || ext == ".tiff"
               || ext == ".png" || ext == ".jpg" || ext == ".jpeg"
               || ext == ".bmp" || ext == ".hdr";
    }



    void refresh_sibling_images(ViewerState& viewer)
    {
        viewer.sibling_images.clear();
        viewer.sibling_index = -1;
        if (viewer.image.path.empty())
            return;

        std::filesystem::path current(viewer.image.path);
        std::error_code ec;
        const std::filesystem::path dir = current.parent_path();
        if (dir.empty() || !std::filesystem::exists(dir, ec))
            return;

        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(dir, ec)) {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            if (!has_supported_image_extension(entry.path()))
                continue;
            viewer.sibling_images.emplace_back(entry.path().string());
        }
        std::sort(viewer.sibling_images.begin(), viewer.sibling_images.end());
        auto it = std::find(viewer.sibling_images.begin(),
                            viewer.sibling_images.end(), current.string());
        if (it != viewer.sibling_images.end())
            viewer.sibling_index = static_cast<int>(
                std::distance(viewer.sibling_images.begin(), it));
    }



    bool pick_sibling_image(const ViewerState& viewer, int delta,
                            std::string& out_path)
    {
        out_path.clear();
        if (viewer.sibling_images.empty() || viewer.sibling_index < 0)
            return false;
        const int count = static_cast<int>(viewer.sibling_images.size());
        int idx = viewer.sibling_index + delta;
        while (idx < 0)
            idx += count;
        idx %= count;
        out_path = viewer.sibling_images[static_cast<size_t>(idx)];
        return !out_path.empty();
    }



    void set_placeholder_status(ViewerState& viewer, const char* action)
    {
        viewer.status_message = Strutil::fmt::format("{} (placeholder)",
                                                     action);
        viewer.last_error.clear();
    }



    void save_as_dialog_action(ViewerState& viewer)
    {
        FileDialog::DialogReply reply
            = FileDialog::save_image_file("", "image.exr");
        if (reply.result == FileDialog::Result::Okay) {
            viewer.status_message = Strutil::fmt::format(
                "Save path selected '{}' (save implementation pending)",
                reply.path);
            viewer.last_error.clear();
        } else if (reply.result == FileDialog::Result::Cancel) {
            viewer.status_message = "Save cancelled";
            viewer.last_error.clear();
        } else {
            viewer.last_error = reply.message;
        }
    }



#if defined(IMIV_BACKEND_VULKAN_GLFW)
    void open_image_dialog_action(VulkanState& vk_state, ViewerState& viewer)
    {
        FileDialog::DialogReply reply = FileDialog::open_image_file("");
        if (reply.result == FileDialog::Result::Okay) {
            load_viewer_image(vk_state, viewer, reply.path);
        } else if (reply.result == FileDialog::Result::Cancel) {
            viewer.status_message = "Open cancelled";
            viewer.last_error.clear();
        } else {
            viewer.last_error = reply.message;
        }
    }



    void reload_current_image_action(VulkanState& vk_state, ViewerState& viewer)
    {
        if (viewer.image.path.empty()) {
            set_placeholder_status(viewer, "Reload image");
            return;
        }
        load_viewer_image(vk_state, viewer, viewer.image.path);
    }



    void close_current_image_action(VulkanState& vk_state, ViewerState& viewer)
    {
        destroy_texture(vk_state, viewer.texture);
        if (!viewer.image.path.empty())
            viewer.toggle_image_path = viewer.image.path;
        viewer.image       = LoadedImage();
        viewer.zoom        = 1.0f;
        viewer.fit_request = true;
        viewer.sibling_images.clear();
        viewer.sibling_index = -1;
        viewer.last_error.clear();
        viewer.status_message = "Closed current image";
    }



    void next_sibling_image_action(VulkanState& vk_state, ViewerState& viewer,
                                   int delta)
    {
        std::string path;
        if (!pick_sibling_image(viewer, delta, path)) {
            set_placeholder_status(viewer, delta < 0 ? "Previous Image"
                                                     : "Next Image");
            return;
        }
        load_viewer_image(vk_state, viewer, path);
    }



    void toggle_image_action(VulkanState& vk_state, ViewerState& viewer)
    {
        if (viewer.toggle_image_path.empty()) {
            set_placeholder_status(viewer, "Toggle image");
            return;
        }
        if (viewer.image.path == viewer.toggle_image_path) {
            if (!pick_sibling_image(viewer, 1, viewer.toggle_image_path)) {
                set_placeholder_status(viewer, "Toggle image");
                return;
            }
        }
        load_viewer_image(vk_state, viewer, viewer.toggle_image_path);
    }
#endif



    void draw_info_window(const ViewerState& viewer, ImGuiID dockspace_id,
                          bool& show_window)
    {
        if (!show_window)
            return;
        if (dockspace_id != 0)
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Image Info", &show_window)) {
            if (viewer.image.path.empty()) {
                ImGui::TextUnformatted("No image loaded.");
            } else {
                ImGui::TextWrapped("%s", viewer.image.path.c_str());
                ImGui::Separator();
                ImGui::Text("Resolution: %d x %d", viewer.image.width,
                            viewer.image.height);
                ImGui::Text("Channels: %d", viewer.image.nchannels);
                ImGui::TextUnformatted(
                    "Metadata/details panel placeholder (iv longinfo parity)");
            }
        }
        ImGui::End();
    }



    void draw_preferences_window(const AppConfig& config,
                                 PlaceholderUiState& ui, ImGuiID dockspace_id,
                                 bool& show_window)
    {
        if (!show_window)
            return;
        if (dockspace_id != 0)
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Preferences", &show_window)) {
            ImGui::Checkbox("Pixel view follows mouse",
                            &ui.pixelview_follows_mouse);
            ImGui::Checkbox("Linear interpolation", &ui.linear_interpolation);
            ImGui::Checkbox("Dark palette", &ui.dark_palette);
            ImGui::Checkbox("Generate mipmaps", &ui.auto_mipmap);
            ImGui::Separator();
            ImGui::InputInt("Image Cache max memory (MB)",
                            &ui.max_memory_ic_mb);
            ImGui::InputInt("Slide show delay (s)", &ui.slide_duration_seconds);
            ImGui::InputInt("# closeup pixels", &ui.closeup_pixels);
            ImGui::InputInt("# closeup avg pixels", &ui.closeup_avg_pixels);
            if (ui.closeup_pixels < 9)
                ui.closeup_pixels = 9;
            if (ui.closeup_avg_pixels < 3)
                ui.closeup_avg_pixels = 3;
            if (ui.closeup_avg_pixels > ui.closeup_pixels)
                ui.closeup_avg_pixels = ui.closeup_pixels;
            ImGui::Separator();
            ImGui::Text("Raw Color startup: %s",
                        config.rawcolor ? "on" : "off");
            ImGui::TextUnformatted(
                "Preference persistence placeholder (QSettings parity pending)");
        }
        ImGui::End();
    }



    void draw_status_window(const ViewerState& viewer,
                            const PlaceholderUiState& ui, ImGuiID dockspace_id,
                            bool show_window)
    {
        if (!show_window)
            return;
        if (dockspace_id != 0)
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Status")) {
            if (!viewer.image.path.empty()) {
                ImGui::Text("Image: %s", viewer.image.path.c_str());
                ImGui::Text("Size: %d x %d, %d channels", viewer.image.width,
                            viewer.image.height, viewer.image.nchannels);
            } else {
                ImGui::TextUnformatted("Image: <none>");
            }
            ImGui::Separator();
            ImGui::Text("Channel: %s", channel_view_name(ui.current_channel));
            ImGui::Text("Color mode: %s", color_mode_name(ui.color_mode));
            ImGui::Text("Subimage: %d  MIP: %d", ui.subimage_index,
                        ui.miplevel_index);
            ImGui::Text("Exposure: %.2f  Gamma: %.2f", ui.exposure, ui.gamma);
            ImGui::Text("OCIO: %s", ui.use_ocio ? "enabled" : "disabled");
        }
        ImGui::End();
    }



    void draw_viewer_ui(const AppConfig& config, ViewerState& viewer,
                        PlaceholderUiState& ui_state, ImGuiID dockspace_id,
                        bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                        ,
                        bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW)
                        ,
                        VulkanState& vk_state
#endif
    )
    {
        bool open_requested    = false;
        bool save_as_requested = false;
        bool reload_requested  = false;
        bool close_requested   = false;
        bool prev_requested    = false;
        bool next_requested    = false;
        bool toggle_requested  = false;
        const bool has_image   = !viewer.image.path.empty();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...", "Ctrl+O"))
                    open_requested = true;

                if (ImGui::BeginMenu("Open recent...")) {
                    ImGui::MenuItem("Recent #1 (placeholder)", nullptr, false,
                                    false);
                    ImGui::MenuItem("Recent #2 (placeholder)", nullptr, false,
                                    false);
                    ImGui::MenuItem("Recent #3 (placeholder)", nullptr, false,
                                    false);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear recent list"))
                        set_placeholder_status(viewer, "Clear recent list");
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Reload image", "Ctrl+R", false, has_image))
                    reload_requested = true;
                if (ImGui::MenuItem("Close image", "Ctrl+W", false, has_image))
                    close_requested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Save As...", "Ctrl+S", false, has_image))
                    save_as_requested = true;
                if (ImGui::MenuItem("Save Window As...", nullptr, false,
                                    has_image))
                    set_placeholder_status(viewer, "Save Window As");
                if (ImGui::MenuItem("Save Selection As...", nullptr, false,
                                    has_image))
                    set_placeholder_status(viewer, "Save Selection As");
                ImGui::Separator();
                if (ImGui::MenuItem("Move to new window", nullptr, false,
                                    has_image))
                    set_placeholder_status(viewer, "Move to new window");
                if (ImGui::MenuItem("Delete from disk", "Delete", false,
                                    has_image))
                    set_placeholder_status(viewer, "Delete from disk");
                ImGui::Separator();
                if (ImGui::MenuItem("Preferences...", "Ctrl+,"))
                    ui_state.show_preferences_window = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl+Q"))
                    request_exit = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Previous Image", "PgUp"))
                    prev_requested = true;
                if (ImGui::MenuItem("Next Image", "PgDown"))
                    next_requested = true;
                if (ImGui::MenuItem("Toggle image", "T"))
                    toggle_requested = true;
                ImGui::MenuItem("Show display/data window borders", nullptr,
                                &ui_state.show_window_guides);
                ImGui::Separator();

                if (ImGui::MenuItem("Zoom In", "Ctrl++", false, has_image))
                    viewer.zoom = std::min(viewer.zoom * 2.0f, 64.0f);
                if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, has_image))
                    viewer.zoom = std::max(viewer.zoom * 0.5f, 0.05f);
                if (ImGui::MenuItem("Normal Size (1:1)", "Ctrl+0", false,
                                    has_image))
                    viewer.zoom = 1.0f;
                if (ImGui::MenuItem("Fit Window to Image", "F", false,
                                    has_image))
                    set_placeholder_status(viewer, "Fit Window to Image");
                if (ImGui::MenuItem("Fit Image to Window", "Alt+F",
                                    ui_state.fit_image_to_window, has_image)) {
                    ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
                    viewer.fit_request = true;
                }
                if (ImGui::MenuItem("Full screen", "Ctrl+F",
                                    ui_state.full_screen_mode)) {
                    ui_state.full_screen_mode = !ui_state.full_screen_mode;
                    set_placeholder_status(viewer, "Full screen toggle");
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Prev Subimage", "<", false, has_image)) {
                    ui_state.subimage_index
                        = std::max(0, ui_state.subimage_index - 1);
                    set_placeholder_status(viewer, "Prev Subimage");
                }
                if (ImGui::MenuItem("Next Subimage", ">", false, has_image)) {
                    ++ui_state.subimage_index;
                    set_placeholder_status(viewer, "Next Subimage");
                }
                if (ImGui::MenuItem("Prev MIP level", nullptr, false,
                                    has_image)) {
                    ui_state.miplevel_index
                        = std::max(0, ui_state.miplevel_index - 1);
                    set_placeholder_status(viewer, "Prev MIP level");
                }
                if (ImGui::MenuItem("Next MIP level", nullptr, false,
                                    has_image)) {
                    ++ui_state.miplevel_index;
                    set_placeholder_status(viewer, "Next MIP level");
                }

                if (ImGui::BeginMenu("Channels")) {
                    if (ImGui::MenuItem("Full Color", "C",
                                        ui_state.current_channel == 0))
                        ui_state.current_channel = 0;
                    if (ImGui::MenuItem("Red", "R",
                                        ui_state.current_channel == 1))
                        ui_state.current_channel = 1;
                    if (ImGui::MenuItem("Green", "G",
                                        ui_state.current_channel == 2))
                        ui_state.current_channel = 2;
                    if (ImGui::MenuItem("Blue", "B",
                                        ui_state.current_channel == 3))
                        ui_state.current_channel = 3;
                    if (ImGui::MenuItem("Alpha", "A",
                                        ui_state.current_channel == 4))
                        ui_state.current_channel = 4;
                    ImGui::Separator();
                    if (ImGui::MenuItem("Prev Channel", ",", false, has_image)) {
                        ui_state.current_channel
                            = std::max(0, ui_state.current_channel - 1);
                    }
                    if (ImGui::MenuItem("Next Channel", ".", false, has_image)) {
                        ui_state.current_channel
                            = std::min(4, ui_state.current_channel + 1);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Color mode")) {
                    if (ImGui::MenuItem("RGBA", nullptr,
                                        ui_state.color_mode == 0))
                        ui_state.color_mode = 0;
                    if (ImGui::MenuItem("RGB", nullptr,
                                        ui_state.color_mode == 1))
                        ui_state.color_mode = 1;
                    if (ImGui::MenuItem("Single channel", "1",
                                        ui_state.color_mode == 2))
                        ui_state.color_mode = 2;
                    if (ImGui::MenuItem("Luminance", "L",
                                        ui_state.color_mode == 3))
                        ui_state.color_mode = 3;
                    if (ImGui::MenuItem("Single channel (Heatmap)", "H",
                                        ui_state.color_mode == 4))
                        ui_state.color_mode = 4;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("OCIO")) {
                    ImGui::MenuItem("Use OCIO", nullptr, &ui_state.use_ocio);
                    if (ImGui::BeginMenu("Image color space")) {
                        if (ImGui::MenuItem("auto", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "auto"))
                            ui_state.ocio_image_color_space = "auto";
                        if (ImGui::MenuItem("scene_linear", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "scene_linear"))
                            ui_state.ocio_image_color_space = "scene_linear";
                        if (ImGui::MenuItem("sRGB", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "sRGB"))
                            ui_state.ocio_image_color_space = "sRGB";
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Display/View")) {
                        if (ImGui::MenuItem("default / default", nullptr,
                                            ui_state.ocio_display == "default"
                                                && ui_state.ocio_view
                                                       == "default")) {
                            ui_state.ocio_display = "default";
                            ui_state.ocio_view    = "default";
                        }
                        if (ImGui::MenuItem("sRGB / Film", nullptr,
                                            ui_state.ocio_display == "sRGB"
                                                && ui_state.ocio_view
                                                       == "Film")) {
                            ui_state.ocio_display = "sRGB";
                            ui_state.ocio_view    = "Film";
                        }
                        if (ImGui::MenuItem("sRGB / Raw", nullptr,
                                            ui_state.ocio_display == "sRGB"
                                                && ui_state.ocio_view
                                                       == "Raw")) {
                            ui_state.ocio_display = "sRGB";
                            ui_state.ocio_view    = "Raw";
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Exposure/gamma")) {
                    if (ImGui::MenuItem("Exposure -1/2 stop", "{"))
                        ui_state.exposure -= 0.5f;
                    if (ImGui::MenuItem("Exposure -1/10 stop", "["))
                        ui_state.exposure -= 0.1f;
                    if (ImGui::MenuItem("Exposure +1/10 stop", "]"))
                        ui_state.exposure += 0.1f;
                    if (ImGui::MenuItem("Exposure +1/2 stop", "}"))
                        ui_state.exposure += 0.5f;
                    if (ImGui::MenuItem("Gamma -0.1", "("))
                        ui_state.gamma = std::max(0.1f, ui_state.gamma - 0.1f);
                    if (ImGui::MenuItem("Gamma +0.1", ")"))
                        ui_state.gamma += 0.1f;
                    if (ImGui::MenuItem("Reset exposure/gamma")) {
                        ui_state.exposure = 0.0f;
                        ui_state.gamma    = 1.0f;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools")) {
                ImGui::MenuItem("Image info...", "Ctrl+I",
                                &ui_state.show_info_window);
                ImGui::MenuItem("Pixel closeup view...", "P",
                                &ui_state.show_pixelview_window);
                ImGui::MenuItem("Toggle area sample", "Ctrl+A",
                                &ui_state.show_area_probe_window);

                if (ImGui::BeginMenu("Slide Show")) {
                    if (ImGui::MenuItem("Start Slide Show", nullptr,
                                        ui_state.slide_show_running)) {
                        ui_state.slide_show_running
                            = !ui_state.slide_show_running;
                        set_placeholder_status(viewer, "Slide show");
                    }
                    if (ImGui::MenuItem("Loop slide show", nullptr,
                                        ui_state.slide_loop))
                        ui_state.slide_loop = !ui_state.slide_loop;
                    if (ImGui::MenuItem("Stop at end", nullptr,
                                        !ui_state.slide_loop))
                        ui_state.slide_loop = false;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Sort")) {
                    if (ImGui::MenuItem("By Name"))
                        set_placeholder_status(viewer, "Sort by name");
                    if (ImGui::MenuItem("By File Path"))
                        set_placeholder_status(viewer, "Sort by file path");
                    if (ImGui::MenuItem("By Image Date"))
                        set_placeholder_status(viewer, "Sort by image date");
                    if (ImGui::MenuItem("By File Date"))
                        set_placeholder_status(viewer, "Sort by file date");
                    if (ImGui::MenuItem("Reverse current order"))
                        set_placeholder_status(viewer, "Sort reverse");
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Rotate Left", "Ctrl+Shift+L"))
                    set_placeholder_status(viewer, "Rotate Left");
                if (ImGui::MenuItem("Rotate Right", "Ctrl+Shift+R"))
                    set_placeholder_status(viewer, "Rotate Right");
                if (ImGui::MenuItem("Flip Horizontal"))
                    set_placeholder_status(viewer, "Flip Horizontal");
                if (ImGui::MenuItem("Flip Vertical"))
                    set_placeholder_status(viewer, "Flip Vertical");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About"))
                    ImGui::OpenPopup("About imiv");
                ImGui::EndMenu();
            }

#if defined(IMGUI_ENABLE_TEST_ENGINE)
            if (show_test_engine_windows && ImGui::BeginMenu("Tests")) {
                ImGui::MenuItem("Show test engine windows", nullptr,
                                show_test_engine_windows);
                ImGui::EndMenu();
            }
#endif
            ImGui::EndMainMenuBar();
        }

        if (open_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            open_image_dialog_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Open image");
#endif
            open_requested = false;
        }
        if (reload_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            reload_current_image_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Reload image");
#endif
            reload_requested = false;
        }
        if (close_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            close_current_image_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Close image");
#endif
            close_requested = false;
        }
        if (prev_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            next_sibling_image_action(vk_state, viewer, -1);
#else
            set_placeholder_status(viewer, "Previous Image");
#endif
            prev_requested = false;
        }
        if (next_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            next_sibling_image_action(vk_state, viewer, 1);
#else
            set_placeholder_status(viewer, "Next Image");
#endif
            next_requested = false;
        }
        if (toggle_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            toggle_image_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Toggle image");
#endif
            toggle_requested = false;
        }
        if (save_as_requested) {
            save_as_dialog_action(viewer);
            save_as_requested = false;
        }

        if (dockspace_id != 0)
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        ImGui::Begin(k_image_window_title);

        if (ImGui::Button("Open..."))
            open_requested = true;
        ImGui::SameLine();
        if (ImGui::Button("Save As..."))
            save_as_requested = true;
        ImGui::SameLine();
        if (ImGui::Button("Info"))
            ui_state.show_info_window = true;
        ImGui::SameLine();
        if (ImGui::Button("Preferences"))
            ui_state.show_preferences_window = true;

        if (open_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            open_image_dialog_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Open image");
#endif
            open_requested = false;
        }
        if (save_as_requested) {
            save_as_dialog_action(viewer);
            save_as_requested = false;
        }

        if (!viewer.status_message.empty())
            ImGui::TextUnformatted(viewer.status_message.c_str());
        if (!viewer.last_error.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
            ImGui::TextWrapped("%s", viewer.last_error.c_str());
            ImGui::PopStyleColor();
        }

        if (!viewer.image.path.empty()) {
            ImGui::Separator();
            ImGui::Text("%s", viewer.image.path.c_str());
            ImGui::Text("%d x %d, %d channels", viewer.image.width,
                        viewer.image.height, viewer.image.nchannels);
            ImGui::Text("Raw Color: %s", config.rawcolor ? "on" : "off");
            ImGui::Text("Channel: %s",
                        channel_view_name(ui_state.current_channel));
            ImGui::Text("Color mode: %s", color_mode_name(ui_state.color_mode));
            ImGui::Text("Subimage: %d  MIP: %d", ui_state.subimage_index,
                        ui_state.miplevel_index);
            ImGui::Text("Exposure: %.2f  Gamma: %.2f", ui_state.exposure,
                        ui_state.gamma);
            ImGui::Text("OCIO: %s  (%s/%s, %s)",
                        ui_state.use_ocio ? "on" : "off",
                        ui_state.ocio_display.c_str(),
                        ui_state.ocio_view.c_str(),
                        ui_state.ocio_image_color_space.c_str());
            if (ImGui::Button("Fit"))
                viewer.fit_request = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0f);
            ImGui::SliderFloat("Zoom", &viewer.zoom, 0.05f, 32.0f, "%.2fx",
                               ImGuiSliderFlags_Logarithmic);

#if defined(IMIV_BACKEND_VULKAN_GLFW)
            PreviewControls preview_controls = {};
            preview_controls.exposure = ui_state.exposure;
            preview_controls.gamma    = ui_state.gamma;
            preview_controls.color_mode = ui_state.color_mode;
            preview_controls.channel    = ui_state.current_channel;
            preview_controls.use_ocio   = ui_state.use_ocio ? 1 : 0;
            std::string preview_error;
            if (!update_preview_texture(vk_state, viewer.texture,
                                        preview_controls, preview_error)) {
                if (!preview_error.empty())
                    viewer.last_error = preview_error;
            }
#endif

            ImGui::BeginChild("Viewport", ImVec2(0, 0), true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            if (viewer.fit_request && viewer.image.width > 0
                && viewer.image.height > 0) {
                const float fit_x = avail.x
                                    / static_cast<float>(viewer.image.width);
                const float fit_y = avail.y
                                    / static_cast<float>(viewer.image.height);
                if (fit_x > 0.0f && fit_y > 0.0f)
                    viewer.zoom = std::max(0.05f, std::min(fit_x, fit_y));
                viewer.fit_request = false;
            }

            const ImVec2 image_size(static_cast<float>(viewer.image.width)
                                        * viewer.zoom,
                                    static_cast<float>(viewer.image.height)
                                        * viewer.zoom);
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            if (viewer.texture.set != VK_NULL_HANDLE)
                ImGui::Image(ImTextureRef(static_cast<ImTextureID>(
                                 reinterpret_cast<uintptr_t>(
                                     viewer.texture.set))),
                             image_size);
            else
                ImGui::TextUnformatted("No texture");
#else
            ImGui::TextUnformatted("No Vulkan backend");
#endif
            if (ImGui::IsItemHovered()) {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    const float scale = (wheel > 0.0f) ? 1.1f : 0.9f;
                    viewer.zoom = std::clamp(viewer.zoom * scale, 0.05f, 64.0f);
                }
            }

            ImGui::EndChild();
        } else {
            ImGui::Separator();
            ImGui::TextUnformatted(
                "Image viewport placeholder is ready. Use File/Open to load an image.");
        }

        ImGui::End();

        draw_info_window(viewer, dockspace_id, ui_state.show_info_window);
        draw_preferences_window(config, ui_state, dockspace_id,
                                ui_state.show_preferences_window);

        if (ui_state.show_pixelview_window) {
            if (dockspace_id != 0)
                ImGui::SetNextWindowDockID(dockspace_id,
                                           ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Pixel Closeup", &ui_state.show_pixelview_window))
                ImGui::TextUnformatted(
                    "Pixel closeup/probe panel placeholder (shader path pending)");
            ImGui::End();
        }

        if (ui_state.show_area_probe_window) {
            if (dockspace_id != 0)
                ImGui::SetNextWindowDockID(dockspace_id,
                                           ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Area Sample", &ui_state.show_area_probe_window))
                ImGui::TextUnformatted(
                    "Area sample statistics placeholder (implementation pending)");
            ImGui::End();
        }

        draw_status_window(viewer, ui_state, dockspace_id,
                           ui_state.show_status_window);

        if (ImGui::BeginPopupModal("About imiv", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("imiv (Dear ImGui port of iv)");
            ImGui::TextUnformatted(
                "UI placeholders are in place; feature implementation follows.");
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

}  // namespace

RenderBackend
default_backend()
{
#if defined(IMIV_BACKEND_METAL_GLFW)
    return RenderBackend::MetalGlfw;
#elif defined(IMIV_BACKEND_VULKAN_GLFW)
    return RenderBackend::VulkanGlfw;
#else
#    error "imiv backend policy macro is not configured"
#endif
}

const char*
backend_name(RenderBackend backend)
{
    switch (backend) {
    case RenderBackend::VulkanGlfw: return "glfw+vulkan";
    case RenderBackend::MetalGlfw: return "glfw+metal";
    }
    return "unknown";
}

int
run(const AppConfig& config)
{
#if !defined(IMIV_BACKEND_VULKAN_GLFW)
    print(stderr, "imiv: backend '{}' is not implemented yet in this build\n",
          backend_name(default_backend()));
    (void)config;
    return EXIT_FAILURE;
#else
    AppConfig run_config = config;
    std::string startup_open_path;
    if (run_config.input_paths.empty()
        && read_env_value("IMIV_IMGUI_TEST_ENGINE_OPEN_PATH", startup_open_path)
        && !startup_open_path.empty()) {
        run_config.input_paths.push_back(startup_open_path);
    }

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    TestEngineConfig test_engine_cfg = gather_test_engine_config();
    TestEngineRuntime test_engine_runtime;
#    else
    bool want_test_engine
        = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML");
#    endif

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        print(stderr, "imiv: glfwInit failed\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "imiv", nullptr, nullptr);
    if (!window) {
        print(stderr, "imiv: failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    if (!glfwVulkanSupported()) {
        print(stderr, "imiv: GLFW reports Vulkan is not supported\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        print(stderr, "imiv: failed to create Dear ImGui context\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VulkanState vk_state;
    ImVector<const char*> instance_extensions;
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions  = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; ++i)
        instance_extensions.push_back(glfw_extensions[i]);

    std::string startup_error;
    if (!setup_vulkan(vk_state, instance_extensions, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VkResult err = glfwCreateWindowSurface(vk_state.instance, window,
                                           vk_state.allocator,
                                           &vk_state.surface);
    if (err != VK_SUCCESS) {
        print(stderr, "imiv: glfwCreateWindowSurface failed\n");
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    int framebuffer_width  = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (!setup_vulkan_window(vk_state, framebuffer_width, framebuffer_height,
                             startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        cleanup_vulkan_window(vk_state);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imiv.ini";
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info    = {};
    init_info.ApiVersion                   = VK_API_VERSION_1_2;
    init_info.Instance                     = vk_state.instance;
    init_info.PhysicalDevice               = vk_state.physical_device;
    init_info.Device                       = vk_state.device;
    init_info.QueueFamily                  = vk_state.queue_family;
    init_info.Queue                        = vk_state.queue;
    init_info.PipelineCache                = vk_state.pipeline_cache;
    init_info.DescriptorPool               = vk_state.descriptor_pool;
    init_info.MinImageCount                = vk_state.min_image_count;
    init_info.ImageCount                   = vk_state.window_data.ImageCount;
    init_info.Allocator                    = vk_state.allocator;
    init_info.PipelineInfoMain.RenderPass  = vk_state.window_data.RenderPass;
    init_info.PipelineInfoMain.Subpass     = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn              = check_vk_result;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        print(stderr, "imiv: ImGui_ImplVulkan_Init failed\n");
        ImGui_ImplGlfw_Shutdown();
        cleanup_vulkan_window(vk_state);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    if (run_config.verbose) {
        print("imiv: bootstrap initialized (backend policy: {})\n",
              backend_name(default_backend()));
        print("imiv: startup queue has {} image path(s)\n",
              run_config.input_paths.size());
        print("imiv: native file dialogs: {}\n",
              FileDialog::available() ? "enabled" : "disabled");
    }

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_imiv_test_runtime = nullptr;
    if (test_engine_cfg.want_test_engine) {
        test_engine_runtime.request_exit = false;
        test_engine_runtime.show_windows = test_engine_cfg.show_windows;
        test_engine_runtime.engine       = ImGuiTestEngine_CreateContext();
        g_imiv_test_runtime              = &test_engine_runtime;

        ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(
            test_engine_runtime.engine);
        test_io.ConfigVerboseLevel        = ImGuiTestVerboseLevel_Info;
        test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
        test_io.ConfigRunSpeed            = ImGuiTestRunSpeed_Normal;
        test_io.ConfigCaptureEnabled      = true;
        test_io.ScreenCaptureFunc         = imiv_vulkan_screen_capture;
        test_io.ScreenCaptureUserData     = &vk_state;
        if (test_engine_cfg.trace || test_engine_cfg.automation_mode)
            test_io.ConfigLogToTTY = true;

        if (test_engine_cfg.junit_xml) {
            std::error_code ec;
            std::filesystem::path junit_path(test_engine_cfg.junit_xml_out);
            if (!junit_path.parent_path().empty())
                std::filesystem::create_directories(junit_path.parent_path(),
                                                    ec);
            test_io.ExportResultsFormat = ImGuiTestEngineExportFormat_JUnitXml;
            test_io.ExportResultsFilename
                = test_engine_cfg.junit_xml_out.c_str();
        }

        ImGuiTestEngine_Start(test_engine_runtime.engine,
                              ImGui::GetCurrentContext());
        if (test_engine_cfg.auto_screenshot) {
            ImGuiTest* smoke = register_imiv_smoke_tests(
                test_engine_runtime.engine);
            ImGuiTestEngine_QueueTest(test_engine_runtime.engine, smoke);
            test_engine_cfg.has_work = true;
        }
        if (test_engine_cfg.layout_dump) {
            ImGuiTest* dump = register_imiv_layout_dump_tests(
                test_engine_runtime.engine);
            ImGuiTestEngine_QueueTest(test_engine_runtime.engine, dump);
            test_engine_cfg.has_work = true;
        }
    }
#    else
    if (want_test_engine) {
        print(stderr,
              "imiv: IMIV_IMGUI_TEST_ENGINE requested but support is not "
              "compiled in. Configure with "
              "-DOIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON.\n");
    }
#    endif

    if (run_config.open_dialog) {
        FileDialog::DialogReply reply = FileDialog::open_image_file("");
        if (reply.result == FileDialog::Result::Okay)
            print("imiv: open dialog selected '{}'\n", reply.path);
        else if (reply.result == FileDialog::Result::Cancel)
            print("imiv: open dialog cancelled\n");
        else
            print(stderr, "imiv: open dialog failed: {}\n", reply.message);
    }
    if (run_config.save_dialog) {
        FileDialog::DialogReply reply
            = FileDialog::save_image_file("", "image.exr");
        if (reply.result == FileDialog::Result::Okay)
            print("imiv: save dialog selected '{}'\n", reply.path);
        else if (reply.result == FileDialog::Result::Cancel)
            print("imiv: save dialog cancelled\n");
        else
            print(stderr, "imiv: save dialog failed: {}\n", reply.message);
    }

    ViewerState viewer;
    PlaceholderUiState ui_state;
    if (!run_config.ocio_display.empty())
        ui_state.ocio_display = run_config.ocio_display;
    if (!run_config.ocio_view.empty())
        ui_state.ocio_view = run_config.ocio_view;
    if (!run_config.ocio_image_color_space.empty())
        ui_state.ocio_image_color_space = run_config.ocio_image_color_space;
    ui_state.use_ocio = (!run_config.ocio_display.empty()
                         || !run_config.ocio_view.empty()
                         || !run_config.ocio_image_color_space.empty());

    if (!run_config.input_paths.empty()) {
        load_viewer_image(vk_state, viewer, run_config.input_paths[0]);
    } else {
        viewer.status_message = "Open an image to start preview";
    }

    bool request_exit = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fb_width  = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0
            && (vk_state.swapchain_rebuild
                || vk_state.window_data.Width != fb_width
                || vk_state.window_data.Height != fb_height)) {
            ImGui_ImplVulkan_SetMinImageCount(vk_state.min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                vk_state.instance, vk_state.physical_device, vk_state.device,
                &vk_state.window_data, vk_state.queue_family,
                vk_state.allocator, fb_width, fb_height,
                vk_state.min_image_count, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            name_window_frame_objects(vk_state);
            vk_state.window_data.FrameIndex = 0;
            vk_state.swapchain_rebuild      = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiID dockspace_id = begin_main_dockspace();
        draw_viewer_ui(run_config, viewer, ui_state, dockspace_id, request_exit
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
                       ,
                       test_engine_runtime.engine
                           ? &test_engine_runtime.show_windows
                           : nullptr
#    endif
                       ,
                       vk_state);
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine && test_engine_runtime.show_windows
            && !test_engine_cfg.automation_mode) {
            ImGuiTestEngine_ShowTestEngineWindows(test_engine_runtime.engine,
                                                  nullptr);
        }
#    endif

        ImGui::Render();
        ImDrawData* draw_data        = ImGui::GetDrawData();
        const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f
                                        || draw_data->DisplaySize.y <= 0.0f);
        vk_state.window_data.ClearValue.color.float32[0] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[1] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[2] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[3] = 1.0f;
        if (!main_is_minimized)
            frame_render(vk_state, draw_data);
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine)
            ImGuiTestEngine_PostSwap(test_engine_runtime.engine);
#    endif
        if (!main_is_minimized)
            frame_present(vk_state);

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine) {
            if (test_engine_runtime.request_exit
                && !test_engine_cfg.exit_on_finish)
                glfwSetWindowShouldClose(window, GLFW_TRUE);

            if (test_engine_cfg.exit_on_finish && test_engine_cfg.has_work) {
                ImGuiTestEngineIO& te_io = ImGuiTestEngine_GetIO(
                    test_engine_runtime.engine);
                if (!te_io.IsRunningTests && !te_io.IsCapturing
                    && ImGuiTestEngine_IsTestQueueEmpty(
                        test_engine_runtime.engine)) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }
#    endif
        if (request_exit)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    err = vkDeviceWaitIdle(vk_state.device);
    check_vk_result(err);

    destroy_texture(vk_state, viewer.texture);
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (test_engine_runtime.engine)
        ImGuiTestEngine_Stop(test_engine_runtime.engine);
#    endif
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (test_engine_runtime.engine) {
        ImGuiTestEngine_DestroyContext(test_engine_runtime.engine);
        test_engine_runtime.engine = nullptr;
    }
    g_imiv_test_runtime = nullptr;
#    endif

    cleanup_vulkan_window(vk_state);
    cleanup_vulkan(vk_state);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
#endif
}

}  // namespace Imiv
