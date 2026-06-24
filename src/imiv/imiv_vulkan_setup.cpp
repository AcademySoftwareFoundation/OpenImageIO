// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_vulkan_resource_utils.h"
#include "imiv_vulkan_shader_utils.h"
#include "imiv_vulkan_types.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#if defined(IMIV_WITH_VULKAN)
#    include <imgui_impl_glfw.h>
#    include <imgui_impl_vulkan.h>
#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>
#endif

#include <OpenImageIO/strutil.h>

#if defined(IMIV_WITH_VULKAN) && defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
    && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
#    include "imiv_preview_frag_spv.h"
#    include "imiv_preview_vert_spv.h"
#    include "imiv_upload_to_rgba16f_fp64_spv.h"
#    include "imiv_upload_to_rgba16f_spv.h"
#    include "imiv_upload_to_rgba32f_fp64_spv.h"
#    include "imiv_upload_to_rgba32f_spv.h"
#endif

using namespace OIIO;

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

#    ifdef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
#        ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
constexpr const char* kPortabilityEnumerationExtensionName
    = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#        else
constexpr const char* kPortabilityEnumerationExtensionName
    = "VK_KHR_portability_enumeration";
#        endif
#    endif

#    ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
constexpr const char* kPortabilitySubsetExtensionName
    = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
#    else
constexpr const char* kPortabilitySubsetExtensionName
    = "VK_KHR_portability_subset";
#    endif

const char*
vk_format_name(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
    case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
    case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
    default: return "VK_FORMAT_UNKNOWN";
    }
}

const char*
vk_color_space_name(VkColorSpaceKHR color_space)
{
    switch (color_space) {
    case VK_COLORSPACE_SRGB_NONLINEAR_KHR:
        return "VK_COLORSPACE_SRGB_NONLINEAR_KHR";
    default: return "VK_COLORSPACE_UNKNOWN";
    }
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



void
append_unique_extension(ImVector<const char*>& extensions,
                        const char* extension_name)
{
    for (const char* existing_name : extensions) {
        if (std::strcmp(existing_name, extension_name) == 0)
            return;
    }
    extensions.push_back(extension_name);
}



bool
is_layer_available(const char* layer_name)
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



uint32_t
choose_instance_api_version()
{
#    ifdef VK_HEADER_VERSION_COMPLETE
    constexpr uint32_t header_version = VK_HEADER_VERSION_COMPLETE;
#    else
    constexpr uint32_t header_version = VK_API_VERSION_1_0;
#    endif

    uint32_t loader_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version
        = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerate_instance_version != nullptr) {
        if (enumerate_instance_version(&loader_version) != VK_SUCCESS)
            loader_version = VK_API_VERSION_1_0;
    }
    return std::min(std::min(header_version, loader_version),
                    VK_API_VERSION_1_3);
}



bool
read_vulkan_limit_override(const char* name, uint32_t& out_value)
{
    out_value         = 0;
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
        return false;
    char* end         = nullptr;
    unsigned long raw = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0'
        || raw > static_cast<unsigned long>(
               std::numeric_limits<uint32_t>::max())) {
        return false;
    }
    out_value = static_cast<uint32_t>(raw);
    return out_value > 0;
}



const char*
physical_device_type_name(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: break;
    }
    return "other";
}



const char*
severity_name(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
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



void
print_message_type(VkDebugUtilsMessageTypeFlagsEXT message_type)
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



const char*
object_type_name(VkObjectType object_type)
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
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "descriptor_set_layout";
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



VKAPI_ATTR VkBool32 VKAPI_CALL
vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
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
            const VkDebugUtilsObjectNameInfoEXT& o = callback_data->pObjects[i];
            const char* object_name                = (o.pObjectName != nullptr
                                       && o.pObjectName[0] != '\0')
                                                         ? o.pObjectName
                                                         : "<unnamed>";
            print(stderr, "imiv: vk object[{}] type={} handle={} name={}\n", i,
                  object_type_name(o.objectType), o.objectHandle, object_name);
        }
    }
    return VK_FALSE;
}



std::string
queue_flags_string(VkQueueFlags flags)
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



bool
cache_queue_family_properties(VulkanState& vk_state, std::string& error_message)
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

    if (vk_state.verbose_logging) {
        print("imiv: Vulkan queue family {} flags={} granularity=({}, {}, "
              "{}) count={}\n",
              vk_state.queue_family,
              queue_flags_string(vk_state.queue_family_properties.queueFlags),
              granularity.width, granularity.height, granularity.depth,
              vk_state.queue_family_properties.queueCount);
    }
    if (vk_state.queue_requires_full_image_copies) {
        print(stderr,
              "imiv: queue family {} requires full-image transfer copies; "
              "enabling conservative ImGui texture upload workaround\n",
              vk_state.queue_family);
    }
    return true;
}



void
populate_debug_messenger_ci(VkDebugUtilsMessengerCreateInfoEXT& ci,
                            bool verbose_output)
{
    ci       = {};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (verbose_output) {
        ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    }
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = vulkan_debug_callback;
}



bool
setup_debug_messenger(VulkanState& vk_state, std::string& error_message)
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
    populate_debug_messenger_ci(ci, vk_state.verbose_validation_output);
    VkResult err = create_fn(vk_state.instance, &ci, vk_state.allocator,
                             &vk_state.debug_messenger);
    if (err != VK_SUCCESS) {
        error_message = "vkCreateDebugUtilsMessengerEXT failed";
        return false;
    }
    return true;
}



bool
has_format_features(VkPhysicalDevice physical_device, VkFormat format,
                    VkFormatFeatureFlags required_features)
{
    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
    return (props.optimalTilingFeatures & required_features)
           == required_features;
}



bool
device_supports_required_extensions(VkPhysicalDevice physical_device,
                                    bool& has_portability_subset,
                                    std::string& error_message)
{
    has_portability_subset = false;

    uint32_t device_extension_count = 0;
    VkResult err = vkEnumerateDeviceExtensionProperties(physical_device,
                                                        nullptr,
                                                        &device_extension_count,
                                                        nullptr);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumerateDeviceExtensionProperties failed";
        return false;
    }

    ImVector<VkExtensionProperties> device_properties;
    device_properties.resize(device_extension_count);
    err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                               &device_extension_count,
                                               device_properties.Data);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumerateDeviceExtensionProperties failed";
        return false;
    }

    if (!is_extension_available(device_properties,
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        return false;

#    ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    has_portability_subset
        = is_extension_available(device_properties,
                                 VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#    endif
    return true;
}



int
score_device_type(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 500;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 250;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 50;
    default: break;
    }
    return 10;
}



int
score_queue_family(const VkQueueFamilyProperties& properties)
{
    int score = 0;
    if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        score += 100;
    if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
        score += 100;
    if ((properties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)
        score += 25;
    score += static_cast<int>(properties.queueCount);
    return score;
}



bool
select_physical_device_and_queue(VulkanState& vk_state,
                                 std::string& error_message)
{
    const VkFormatFeatureFlags required_compute_output_features
        = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    uint32_t device_count = 0;
    VkResult err = vkEnumeratePhysicalDevices(vk_state.instance, &device_count,
                                              nullptr);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumeratePhysicalDevices failed";
        return false;
    }
    if (device_count == 0) {
        error_message = "no Vulkan physical device found";
        return false;
    }

    ImVector<VkPhysicalDevice> devices;
    devices.resize(device_count);
    err = vkEnumeratePhysicalDevices(vk_state.instance, &device_count,
                                     devices.Data);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumeratePhysicalDevices failed";
        return false;
    }

    int best_score               = std::numeric_limits<int>::min();
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    uint32_t best_queue_family   = static_cast<uint32_t>(-1);
    VkPhysicalDeviceProperties best_properties = {};

    for (VkPhysicalDevice device : devices) {
        bool has_portability_subset = false;
        std::string extension_error;
        if (!device_supports_required_extensions(device, has_portability_subset,
                                                 extension_error)) {
            if (!extension_error.empty()) {
                error_message = extension_error;
                return false;
            }
            continue;
        }

        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(device, &properties);

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(device, &features);

        if (!has_format_features(device, VK_FORMAT_R16G16B16A16_SFLOAT,
                                 required_compute_output_features)
            && !has_format_features(device, VK_FORMAT_R32G32B32A32_SFLOAT,
                                    required_compute_output_features)) {
            continue;
        }

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 nullptr);
        if (queue_family_count == 0)
            continue;

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 queue_families.data());

        for (uint32_t family_index = 0; family_index < queue_family_count;
             ++family_index) {
            const VkQueueFamilyProperties& family_properties
                = queue_families[family_index];
            if ((family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0
                || (family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0
                || family_properties.queueCount == 0) {
                continue;
            }

            VkBool32 supports_present = VK_FALSE;
            err = vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index,
                                                       vk_state.surface,
                                                       &supports_present);
            if (err != VK_SUCCESS) {
                error_message = "vkGetPhysicalDeviceSurfaceSupportKHR failed";
                return false;
            }
            if (supports_present != VK_TRUE)
                continue;

            int score = score_device_type(properties.deviceType);
            score += score_queue_family(family_properties);
            if (features.shaderFloat64 == VK_TRUE)
                score += 50;
            if (has_portability_subset)
                score += 5;

            if (score > best_score) {
                best_score        = score;
                best_device       = device;
                best_queue_family = family_index;
                best_properties   = properties;
            }
        }
    }

    if (best_device == VK_NULL_HANDLE
        || best_queue_family == static_cast<uint32_t>(-1)) {
        error_message
            = "no Vulkan device/queue family supports graphics+compute+present for the window surface";
        return false;
    }

    vk_state.physical_device = best_device;
    vk_state.queue_family    = best_queue_family;
    vk_state.max_image_dimension_2d = best_properties.limits.maxImageDimension2D;
    vk_state.max_storage_buffer_range
        = best_properties.limits.maxStorageBufferRange;
    vk_state.min_storage_buffer_offset_alignment = static_cast<uint32_t>(
        std::max<VkDeviceSize>(
            1, best_properties.limits.minStorageBufferOffsetAlignment));
    uint32_t storage_buffer_override = 0;
    if (read_vulkan_limit_override(
            "IMIV_VULKAN_MAX_STORAGE_BUFFER_RANGE_OVERRIDE",
            storage_buffer_override)) {
        vk_state.max_storage_buffer_range
            = std::min(vk_state.max_storage_buffer_range,
                       storage_buffer_override);
    }
    if (!cache_queue_family_properties(vk_state, error_message))
        return false;

    if (vk_state.verbose_logging) {
        print("imiv: selected Vulkan device='{}' type={} api={}.{}.{} "
              "queue_family={} maxImageDimension2D={} "
              "maxStorageBufferRange={} minStorageBufferOffsetAlignment={}\n",
              best_properties.deviceName,
              physical_device_type_name(best_properties.deviceType),
              VK_API_VERSION_MAJOR(best_properties.apiVersion),
              VK_API_VERSION_MINOR(best_properties.apiVersion),
              VK_API_VERSION_PATCH(best_properties.apiVersion),
              vk_state.queue_family, vk_state.max_image_dimension_2d,
              vk_state.max_storage_buffer_range,
              vk_state.min_storage_buffer_offset_alignment);
    }
    return true;
}



bool
create_compute_pipeline(VulkanState& vk_state, const uint32_t* shader_words,
                        size_t shader_word_count,
                        const std::string& shader_path,
                        const char* shader_label, const char* debug_name,
                        VkPipeline& out_pipeline, std::string& error_message)
{
    VkShaderModule shader_module = VK_NULL_HANDLE;
    out_pipeline                 = VK_NULL_HANDLE;
    const char* module_name      = (shader_words != nullptr
                               && shader_word_count != 0)
                                       ? shader_label
                                       : shader_path.c_str();
    if (!create_shader_module_from_embedded_or_file(
            vk_state.device, vk_state.allocator, shader_words,
            shader_word_count, shader_path, shader_label, shader_module,
            error_message)) {
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
    const VkResult err
        = vkCreateComputePipelines(vk_state.device, vk_state.pipeline_cache, 1,
                                   &pipeline_ci, vk_state.allocator,
                                   &out_pipeline);
    vkDestroyShaderModule(vk_state.device, shader_module, vk_state.allocator);
    if (err != VK_SUCCESS) {
        error_message
            = Strutil::fmt::format("vkCreateComputePipelines failed for '{}'",
                                   module_name);
        out_pipeline = VK_NULL_HANDLE;
        return false;
    }

    set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE, out_pipeline,
                       debug_name);
    return true;
}

static void
destroy_shader_module_resource(VulkanState& vk_state,
                               VkShaderModule& shader_module)
{
    if (shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk_state.device, shader_module,
                              vk_state.allocator);
        shader_module = VK_NULL_HANDLE;
    }
}

static void
destroy_pipeline_resource(VulkanState& vk_state, VkPipeline& pipeline)
{
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk_state.device, pipeline, vk_state.allocator);
        pipeline = VK_NULL_HANDLE;
    }
}

static void
destroy_pipeline_layout_resource(VulkanState& vk_state,
                                 VkPipelineLayout& pipeline_layout)
{
    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk_state.device, pipeline_layout,
                                vk_state.allocator);
        pipeline_layout = VK_NULL_HANDLE;
    }
}

static void
destroy_descriptor_set_layout_resource(
    VulkanState& vk_state, VkDescriptorSetLayout& descriptor_set_layout)
{
    if (descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk_state.device, descriptor_set_layout,
                                     vk_state.allocator);
        descriptor_set_layout = VK_NULL_HANDLE;
    }
}

static void
destroy_descriptor_pool_resource(VulkanState& vk_state,
                                 VkDescriptorPool& descriptor_pool)
{
    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk_state.device, descriptor_pool,
                                vk_state.allocator);
        descriptor_pool = VK_NULL_HANDLE;
    }
}

static void
destroy_render_pass_resource(VulkanState& vk_state, VkRenderPass& render_pass)
{
    if (render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vk_state.device, render_pass, vk_state.allocator);
        render_pass = VK_NULL_HANDLE;
    }
}



void
destroy_preview_resources(VulkanState& vk_state)
{
    destroy_ocio_preview_resources(vk_state);
    destroy_pipeline_resource(vk_state, vk_state.preview_pipeline);
    destroy_pipeline_layout_resource(vk_state,
                                     vk_state.preview_pipeline_layout);
    destroy_descriptor_set_layout_resource(
        vk_state, vk_state.preview_descriptor_set_layout);
    destroy_descriptor_pool_resource(vk_state,
                                     vk_state.preview_descriptor_pool);
    destroy_render_pass_resource(vk_state, vk_state.preview_render_pass);
}



bool
init_preview_resources(VulkanState& vk_state, std::string& error_message)
{
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) \
          && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
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
    attachment.format                  = vk_state.compute_output_format;
    attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment            = 0;
    color_ref.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_ref;

    VkRenderPassCreateInfo render_pass_ci = {};
    render_pass_ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments    = &attachment;
    render_pass_ci.subpassCount    = 1;
    render_pass_ci.pSubpasses      = &subpass;
    const VkDescriptorPoolSize preview_pool_sizes[]
        = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 } };
    const VkDescriptorSetLayoutBinding preview_bindings[]
        = { make_descriptor_set_layout_binding(
            0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT) };
    VkPushConstantRange preview_push = {};
    preview_push.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
    preview_push.offset              = 0;
    preview_push.size                = sizeof(PreviewPushConstants);

    const std::string shader_vert = std::string(IMIV_SHADER_DIR)
                                    + "/imiv_preview.vert.spv";
    const std::string shader_frag = std::string(IMIV_SHADER_DIR)
                                    + "/imiv_preview.frag.spv";
#        if defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
            && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
    const uint32_t* shader_vert_words   = g_imiv_preview_vert_spv;
    const size_t shader_vert_word_count = g_imiv_preview_vert_spv_word_count;
    const uint32_t* shader_frag_words   = g_imiv_preview_frag_spv;
    const size_t shader_frag_word_count = g_imiv_preview_frag_spv_word_count;
#        else
    const uint32_t* shader_vert_words   = nullptr;
    const size_t shader_vert_word_count = 0;
    const uint32_t* shader_frag_words   = nullptr;
    const size_t shader_frag_word_count = 0;
#        endif
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VkShaderModule frag_module = VK_NULL_HANDLE;
    bool ok                    = false;
    do {
        VkResult err = vkCreateRenderPass(vk_state.device, &render_pass_ci,
                                          vk_state.allocator,
                                          &vk_state.preview_render_pass);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateRenderPass failed for preview pipeline";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS,
                           vk_state.preview_render_pass,
                           "imiv.preview.render_pass");

        if (!create_descriptor_pool_resource(
                vk_state, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                256, preview_pool_sizes,
                static_cast<uint32_t>(IM_ARRAYSIZE(preview_pool_sizes)),
                vk_state.preview_descriptor_pool,
                "vkCreateDescriptorPool failed for preview",
                "imiv.preview.descriptor_pool", error_message)) {
            break;
        }
        if (!create_descriptor_set_layout_resource(
                vk_state, preview_bindings,
                static_cast<uint32_t>(IM_ARRAYSIZE(preview_bindings)),
                vk_state.preview_descriptor_set_layout,
                "vkCreateDescriptorSetLayout failed for preview",
                "imiv.preview.set_layout", error_message)) {
            break;
        }
        if (!create_pipeline_layout_resource(
                vk_state, &vk_state.preview_descriptor_set_layout, 1,
                &preview_push, 1, vk_state.preview_pipeline_layout,
                "vkCreatePipelineLayout failed for preview",
                "imiv.preview.pipeline_layout", error_message)) {
            break;
        }
        if (!create_shader_module_from_embedded_or_file(
                vk_state.device, vk_state.allocator, shader_vert_words,
                shader_vert_word_count, shader_vert, "imiv.preview.vert",
                vert_module, error_message)) {
            break;
        }
        if (!create_shader_module_from_embedded_or_file(
                vk_state.device, vk_state.allocator, shader_frag_words,
                shader_frag_word_count, shader_frag, "imiv.preview.frag",
                frag_module, error_message)) {
            break;
        }
        if (!create_fullscreen_preview_pipeline(
                vk_state, vk_state.preview_render_pass,
                vk_state.preview_pipeline_layout, vert_module, frag_module,
                "imiv.preview.pipeline",
                "vkCreateGraphicsPipelines failed for preview",
                vk_state.preview_pipeline, error_message)) {
            break;
        }
        ok = true;
    } while (false);

    destroy_shader_module_resource(vk_state, frag_module);
    destroy_shader_module_resource(vk_state, vert_module);
    if (!ok)
        destroy_preview_resources(vk_state);
    return ok;
#    endif
}



void
destroy_compute_upload_resources(VulkanState& vk_state)
{
    destroy_pipeline_resource(vk_state, vk_state.compute_pipeline_fp64);
    destroy_pipeline_resource(vk_state, vk_state.compute_pipeline);
    destroy_pipeline_layout_resource(vk_state,
                                     vk_state.compute_pipeline_layout);
    destroy_descriptor_set_layout_resource(
        vk_state, vk_state.compute_descriptor_set_layout);
    destroy_descriptor_pool_resource(vk_state,
                                     vk_state.compute_descriptor_pool);
    vk_state.compute_output_format = VK_FORMAT_UNDEFINED;
    vk_state.compute_upload_ready  = false;
}



bool
init_compute_upload_resources(VulkanState& vk_state, std::string& error_message)
{
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) \
          && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
    error_message = "compute upload shaders were not generated at build time";
    return false;
#    else
    destroy_compute_upload_resources(vk_state);

    if ((vk_state.queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
        == 0) {
        error_message = "selected Vulkan queue family does not support compute";
        return false;
    }

    VkPhysicalDeviceFeatures features = {};
    vkGetPhysicalDeviceFeatures(vk_state.physical_device, &features);
    vk_state.compute_supports_float64 = (features.shaderFloat64 == VK_TRUE);

    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
                                          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    std::string shader_path;
    std::string shader_path_fp64;
    const uint32_t* shader_words      = nullptr;
    size_t shader_word_count          = 0;
    const uint32_t* shader_words_fp64 = nullptr;
    size_t shader_word_count_fp64     = 0;
    if (has_format_features(vk_state.physical_device,
                            VK_FORMAT_R16G16B16A16_SFLOAT, required)) {
        vk_state.compute_output_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        shader_path                    = std::string(IMIV_SHADER_DIR)
                      + "/imiv_upload_to_rgba16f.comp.spv";
        shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                           + "/imiv_upload_to_rgba16f_fp64.comp.spv";
#        if defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
            && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
        shader_words           = g_imiv_upload_to_rgba16f_spv;
        shader_word_count      = g_imiv_upload_to_rgba16f_spv_word_count;
        shader_words_fp64      = g_imiv_upload_to_rgba16f_fp64_spv;
        shader_word_count_fp64 = g_imiv_upload_to_rgba16f_fp64_spv_word_count;
#        endif
    } else if (has_format_features(vk_state.physical_device,
                                   VK_FORMAT_R32G32B32A32_SFLOAT, required)) {
        vk_state.compute_output_format = VK_FORMAT_R32G32B32A32_SFLOAT;
        shader_path                    = std::string(IMIV_SHADER_DIR)
                      + "/imiv_upload_to_rgba32f.comp.spv";
        shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                           + "/imiv_upload_to_rgba32f_fp64.comp.spv";
#        if defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
            && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
        shader_words           = g_imiv_upload_to_rgba32f_spv;
        shader_word_count      = g_imiv_upload_to_rgba32f_spv_word_count;
        shader_words_fp64      = g_imiv_upload_to_rgba32f_fp64_spv;
        shader_word_count_fp64 = g_imiv_upload_to_rgba32f_fp64_spv_word_count;
#        endif
    } else {
        error_message
            = "no compute output format support for rgba16f/rgba32f storage image";
        return false;
    }

    const VkDescriptorPoolSize pool_sizes[]
        = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64 } };
    const VkDescriptorSetLayoutBinding bindings[] = {
        make_descriptor_set_layout_binding(
            0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
            VK_SHADER_STAGE_COMPUTE_BIT),
        make_descriptor_set_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VK_SHADER_STAGE_COMPUTE_BIT)
    };

    VkPushConstantRange push_range = {};
    push_range.stageFlags          = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset              = 0;
    push_range.size                = sizeof(UploadComputePushConstants);
    bool ok                        = false;
    do {
        if (!create_descriptor_pool_resource(
                vk_state, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 64,
                pool_sizes, static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes)),
                vk_state.compute_descriptor_pool,
                "vkCreateDescriptorPool failed for compute upload",
                "imiv.compute_upload.descriptor_pool", error_message)) {
            break;
        }
        if (!create_descriptor_set_layout_resource(
                vk_state, bindings,
                static_cast<uint32_t>(IM_ARRAYSIZE(bindings)),
                vk_state.compute_descriptor_set_layout,
                "vkCreateDescriptorSetLayout failed for compute upload",
                "imiv.compute_upload.set_layout", error_message)) {
            break;
        }
        if (!create_pipeline_layout_resource(
                vk_state, &vk_state.compute_descriptor_set_layout, 1,
                &push_range, 1, vk_state.compute_pipeline_layout,
                "vkCreatePipelineLayout failed for compute upload",
                "imiv.compute_upload.pipeline_layout", error_message)) {
            break;
        }
        if (!create_compute_pipeline(vk_state, shader_words, shader_word_count,
                                     shader_path, "imiv.upload_to_rgba",
                                     "imiv.compute_upload.pipeline",
                                     vk_state.compute_pipeline,
                                     error_message)) {
            break;
        }
        ok = true;
    } while (false);
    if (!ok) {
        destroy_compute_upload_resources(vk_state);
        return false;
    }

    if (vk_state.compute_supports_float64) {
        std::string fp64_error;
        if (!create_compute_pipeline(vk_state, shader_words_fp64,
                                     shader_word_count_fp64, shader_path_fp64,
                                     "imiv.upload_to_rgba.fp64",
                                     "imiv.compute_upload.pipeline_fp64",
                                     vk_state.compute_pipeline_fp64,
                                     fp64_error)) {
            print(stderr,
                  "imiv: fp64 compute pipeline unavailable, will fallback "
                  "double->float on CPU: {}\n",
                  fp64_error);
        }
    }

    vk_state.compute_upload_ready = true;
    if (vk_state.verbose_logging) {
        print(
            "imiv: compute upload ready output_format={} shader={} float64_support={} fp64_pipeline={}\n",
            static_cast<int>(vk_state.compute_output_format), shader_path,
            vk_state.compute_supports_float64 ? "yes" : "no",
            vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE ? "yes" : "no");
    }
    return true;
#    endif
}



bool
setup_vulkan_instance(VulkanState& vk_state,
                      ImVector<const char*>& instance_extensions,
                      std::string& error_message)
{
    VkResult err;

    VkInstanceCreateInfo instance_ci = {};
    instance_ci.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
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
#    ifdef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
    if (is_extension_available(instance_properties,
                               kPortabilityEnumerationExtensionName)) {
        append_unique_extension(instance_extensions,
                                kPortabilityEnumerationExtensionName);
        instance_ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
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
    vk_state.api_version              = choose_instance_api_version();
    app_info.apiVersion               = vk_state.api_version;
    instance_ci.pApplicationInfo      = &app_info;
    instance_ci.enabledExtensionCount = static_cast<uint32_t>(
        instance_extensions.Size);
    instance_ci.ppEnabledExtensionNames = instance_extensions.Data;
    instance_ci.enabledLayerCount = static_cast<uint32_t>(instance_layers.Size);
    instance_ci.ppEnabledLayerNames = instance_layers.Data;

    VkDebugUtilsMessengerCreateInfoEXT debug_ci = {};
    if (vk_state.validation_layer_enabled && vk_state.debug_utils_enabled) {
        populate_debug_messenger_ci(debug_ci,
                                    vk_state.verbose_validation_output);
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
            if (vk_state.verbose_validation_output) {
                print("imiv: Vulkan validation enabled with verbose debug "
                      "utils output\n");
            } else if (vk_state.verbose_logging) {
                print("imiv: Vulkan validation enabled with warnings/errors "
                      "only\n");
            }
        } else {
            if (vk_state.verbose_logging) {
                print("imiv: Vulkan validation enabled without debug utils "
                      "messenger\n");
            }
        }
    }
    return true;
}

bool
setup_vulkan_device(VulkanState& vk_state, std::string& error_message)
{
    VkResult err;

    if (!select_physical_device_and_queue(vk_state, error_message))
        return false;

    ImVector<const char*> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    uint32_t device_extension_count = 0;
    err = vkEnumerateDeviceExtensionProperties(vk_state.physical_device,
                                               nullptr, &device_extension_count,
                                               nullptr);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumerateDeviceExtensionProperties failed";
        return false;
    }
    ImVector<VkExtensionProperties> device_properties;
    device_properties.resize(device_extension_count);
    err = vkEnumerateDeviceExtensionProperties(vk_state.physical_device,
                                               nullptr, &device_extension_count,
                                               device_properties.Data);
    if (err != VK_SUCCESS) {
        error_message = "vkEnumerateDeviceExtensionProperties failed";
        return false;
    }
    if (is_extension_available(device_properties,
                               kPortabilitySubsetExtensionName)) {
        append_unique_extension(device_extensions,
                                kPortabilitySubsetExtensionName);
    }

    const float queue_priority[]     = { 1.0f };
    VkDeviceQueueCreateInfo queue_ci = {};
    queue_ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex = vk_state.queue_family;
    queue_ci.queueCount       = 1;
    queue_ci.pQueuePriorities = queue_priority;

    VkPhysicalDeviceFeatures supported_features = {};
    vkGetPhysicalDeviceFeatures(vk_state.physical_device, &supported_features);
    VkPhysicalDeviceFeatures enabled_features = {};
    if (supported_features.shaderFloat64 == VK_TRUE) {
        enabled_features.shaderFloat64    = VK_TRUE;
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

    const VkDescriptorPoolSize pool_sizes[]
        = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 } };
    if (!create_descriptor_pool_resource(
            vk_state, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1024,
            pool_sizes, static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes)),
            vk_state.descriptor_pool, "vkCreateDescriptorPool failed",
            "imiv.main.descriptor_pool", error_message)) {
        return false;
    }

    if (!init_compute_upload_resources(vk_state, error_message))
        return false;
    if (!init_preview_resources(vk_state, error_message))
        return false;

    return true;
}

bool
setup_vulkan_window(VulkanState& vk_state, int width, int height,
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

    const VkFormat request_surface_formats_8bit[]
        = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkFormat request_surface_formats_10bit[]
        = { VK_FORMAT_A2B10G10R10_UNORM_PACK32,
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8_UNORM,
            VK_FORMAT_R8G8B8_UNORM };
    const bool request_10bit = vk_state.requested_display_format
                               == DisplayFormatPreference::Rgb10A2;
    const VkFormat* request_surface_formats
        = request_10bit ? request_surface_formats_10bit
                        : request_surface_formats_8bit;
    const int request_surface_format_count
        = request_10bit
              ? static_cast<int>(IM_ARRAYSIZE(request_surface_formats_10bit))
              : static_cast<int>(IM_ARRAYSIZE(request_surface_formats_8bit));
    const VkColorSpaceKHR request_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    vk_state.window_data.Surface       = vk_state.surface;
    vk_state.window_data.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        vk_state.physical_device, vk_state.window_data.Surface,
        request_surface_formats, request_surface_format_count,
        request_color_space);

    VkSurfaceCapabilitiesKHR surface_caps = {};
    VkResult caps_err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vk_state.physical_device, vk_state.surface, &surface_caps);
    if (caps_err != VK_SUCCESS) {
        check_vk_result(caps_err);
        error_message = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed";
        return false;
    }
    vk_state.window_image_usage = 0;
    if (surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        vk_state.window_image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    else if (vk_state.verbose_logging)
        print("imiv: Vulkan swapchain does not support transfer-src; "
              "screenshots will be unavailable\n");

    if (vk_state.verbose_logging) {
        const bool got_10bit = vk_state.window_data.SurfaceFormat.format
                                   == VK_FORMAT_A2B10G10R10_UNORM_PACK32
                               || vk_state.window_data.SurfaceFormat.format
                                      == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        print("imiv: Vulkan display format requested={} selected={} "
              "color_space={}{}\n",
              display_format_cli_name(vk_state.requested_display_format),
              vk_format_name(vk_state.window_data.SurfaceFormat.format),
              vk_color_space_name(vk_state.window_data.SurfaceFormat.colorSpace),
              (request_10bit && !got_10bit) ? " (fell back)" : "");
    }

    const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    vk_state.window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        vk_state.physical_device, vk_state.window_data.Surface, present_modes,
        static_cast<int>(IM_ARRAYSIZE(present_modes)));

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        vk_state.instance, vk_state.physical_device, vk_state.device,
        &vk_state.window_data, vk_state.queue_family, vk_state.allocator, width,
        height, vk_state.min_image_count, vk_state.window_image_usage);
    name_window_frame_objects(vk_state);
    return true;
}

void
destroy_vulkan_surface(VulkanState& vk_state)
{
    if (vk_state.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk_state.instance, vk_state.surface,
                            vk_state.allocator);
        vk_state.surface = VK_NULL_HANDLE;
    }
}

void
cleanup_vulkan_window(VulkanState& vk_state)
{
    if (vk_state.window_data.Swapchain != VK_NULL_HANDLE) {
        ImGui_ImplVulkanH_DestroyWindow(vk_state.instance, vk_state.device,
                                        &vk_state.window_data,
                                        vk_state.allocator);
        vk_state.window_data = ImGui_ImplVulkanH_Window();
    }
    destroy_vulkan_surface(vk_state);
}



void
cleanup_vulkan(VulkanState& vk_state)
{
    if (vk_state.device != VK_NULL_HANDLE)
        drain_retired_textures(vk_state, true);
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
    if (vk_state.device != VK_NULL_HANDLE)
        destroy_immediate_submit_resources(vk_state);
    destroy_descriptor_pool_resource(vk_state, vk_state.descriptor_pool);
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

#endif

}  // namespace Imiv
