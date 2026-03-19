// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_build_config.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <OpenImageIO/typedesc.h>
#include <imgui.h>

#if defined(IMIV_WITH_VULKAN)
#    include <imgui_impl_vulkan.h>
#endif

namespace Imiv {

enum class UploadDataType : uint32_t {
    UInt8   = 0,
    UInt16  = 1,
    UInt32  = 2,
    Half    = 3,
    Float   = 4,
    Double  = 5,
    Unknown = 255
};

size_t
upload_data_type_size(UploadDataType type);
const char*
upload_data_type_name(UploadDataType type);
OIIO::TypeDesc
upload_data_type_to_typedesc(UploadDataType type);
bool
map_spec_type_to_upload(OIIO::TypeDesc spec_type, UploadDataType& upload_type,
                        OIIO::TypeDesc& read_format);

struct LoadedImage {
    std::string path;
    std::string metadata_color_space;
    int width              = 0;
    int height             = 0;
    int orientation        = 1;
    int nchannels          = 0;
    int subimage           = 0;
    int miplevel           = 0;
    int nsubimages         = 1;
    int nmiplevels         = 1;
    UploadDataType type    = UploadDataType::Unknown;
    size_t channel_bytes   = 0;
    size_t row_pitch_bytes = 0;
    std::vector<unsigned char> pixels;
    std::vector<std::string> channel_names;
    std::vector<std::pair<std::string, std::string>> longinfo_rows;
};

#if defined(IMIV_WITH_VULKAN)

struct PlaceholderUiState;
struct OcioShaderRuntime;

struct PreviewControls {
    float exposure           = 0.0f;
    float gamma              = 1.0f;
    float offset             = 0.0f;
    int color_mode           = 0;
    int channel              = 0;
    int use_ocio             = 0;
    int orientation          = 1;
    int linear_interpolation = 0;
};

struct VulkanTexture {
    VkImage source_image         = VK_NULL_HANDLE;
    VkImageView source_view      = VK_NULL_HANDLE;
    VkDeviceMemory source_memory = VK_NULL_HANDLE;

    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkFramebuffer preview_framebuffer       = VK_NULL_HANDLE;
    VkDescriptorSet preview_source_set      = VK_NULL_HANDLE;
    VkSampler sampler                       = VK_NULL_HANDLE;
    VkSampler nearest_mag_sampler           = VK_NULL_HANDLE;
    VkSampler pixelview_sampler             = VK_NULL_HANDLE;
    VkDescriptorSet set                     = VK_NULL_HANDLE;
    VkDescriptorSet nearest_mag_set         = VK_NULL_HANDLE;
    VkDescriptorSet pixelview_set           = VK_NULL_HANDLE;
    VkBuffer upload_staging_buffer          = VK_NULL_HANDLE;
    VkDeviceMemory upload_staging_memory    = VK_NULL_HANDLE;
    VkBuffer upload_source_buffer           = VK_NULL_HANDLE;
    VkDeviceMemory upload_source_memory     = VK_NULL_HANDLE;
    VkDescriptorSet upload_compute_set      = VK_NULL_HANDLE;
    VkCommandPool upload_command_pool       = VK_NULL_HANDLE;
    VkCommandBuffer upload_command_buffer   = VK_NULL_HANDLE;
    VkFence upload_submit_fence             = VK_NULL_HANDLE;
    VkCommandPool preview_command_pool      = VK_NULL_HANDLE;
    VkCommandBuffer preview_command_buffer  = VK_NULL_HANDLE;
    VkFence preview_submit_fence            = VK_NULL_HANDLE;
    int width                               = 0;
    int height                              = 0;
    bool source_ready                       = false;
    bool upload_submit_pending              = false;
    bool preview_initialized                = false;
    bool preview_submit_pending             = false;
    bool preview_dirty                      = false;
    bool preview_params_valid               = false;
    PreviewControls last_preview_controls   = {};
    PreviewControls preview_submit_controls = {};

    VulkanTexture()                                = default;
    VulkanTexture(const VulkanTexture&)            = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    VulkanTexture(VulkanTexture&& other) noexcept { *this = std::move(other); }

    VulkanTexture& operator=(VulkanTexture&& other) noexcept
    {
        if (this == &other)
            return *this;
        source_image  = std::exchange(other.source_image, VK_NULL_HANDLE);
        source_view   = std::exchange(other.source_view, VK_NULL_HANDLE);
        source_memory = std::exchange(other.source_memory, VK_NULL_HANDLE);
        image         = std::exchange(other.image, VK_NULL_HANDLE);
        view          = std::exchange(other.view, VK_NULL_HANDLE);
        memory        = std::exchange(other.memory, VK_NULL_HANDLE);
        preview_framebuffer = std::exchange(other.preview_framebuffer,
                                            VK_NULL_HANDLE);
        preview_source_set  = std::exchange(other.preview_source_set,
                                            VK_NULL_HANDLE);
        sampler             = std::exchange(other.sampler, VK_NULL_HANDLE);
        nearest_mag_sampler = std::exchange(other.nearest_mag_sampler,
                                            VK_NULL_HANDLE);
        pixelview_sampler   = std::exchange(other.pixelview_sampler,
                                            VK_NULL_HANDLE);
        set                 = std::exchange(other.set, VK_NULL_HANDLE);
        nearest_mag_set = std::exchange(other.nearest_mag_set, VK_NULL_HANDLE);
        pixelview_set   = std::exchange(other.pixelview_set, VK_NULL_HANDLE);
        upload_staging_buffer  = std::exchange(other.upload_staging_buffer,
                                               VK_NULL_HANDLE);
        upload_staging_memory  = std::exchange(other.upload_staging_memory,
                                               VK_NULL_HANDLE);
        upload_source_buffer   = std::exchange(other.upload_source_buffer,
                                               VK_NULL_HANDLE);
        upload_source_memory   = std::exchange(other.upload_source_memory,
                                               VK_NULL_HANDLE);
        upload_compute_set     = std::exchange(other.upload_compute_set,
                                               VK_NULL_HANDLE);
        upload_command_pool    = std::exchange(other.upload_command_pool,
                                               VK_NULL_HANDLE);
        upload_command_buffer  = std::exchange(other.upload_command_buffer,
                                               VK_NULL_HANDLE);
        upload_submit_fence    = std::exchange(other.upload_submit_fence,
                                               VK_NULL_HANDLE);
        preview_command_pool   = std::exchange(other.preview_command_pool,
                                               VK_NULL_HANDLE);
        preview_command_buffer = std::exchange(other.preview_command_buffer,
                                               VK_NULL_HANDLE);
        preview_submit_fence   = std::exchange(other.preview_submit_fence,
                                               VK_NULL_HANDLE);
        width                  = std::exchange(other.width, 0);
        height                 = std::exchange(other.height, 0);
        source_ready           = std::exchange(other.source_ready, false);
        upload_submit_pending  = std::exchange(other.upload_submit_pending,
                                               false);
        preview_initialized = std::exchange(other.preview_initialized, false);
        preview_submit_pending = std::exchange(other.preview_submit_pending,
                                               false);
        preview_dirty          = std::exchange(other.preview_dirty, false);
        preview_params_valid = std::exchange(other.preview_params_valid, false);
        last_preview_controls = std::exchange(other.last_preview_controls, {});
        preview_submit_controls = std::exchange(other.preview_submit_controls,
                                                {});
        return *this;
    }
};

struct UploadComputePushConstants {
    uint32_t width           = 0;
    uint32_t height          = 0;
    uint32_t row_pitch_bytes = 0;
    uint32_t pixel_stride    = 0;
    uint32_t channel_count   = 0;
    uint32_t data_type       = 0;
};

struct PreviewPushConstants {
    float exposure      = 0.0f;
    float gamma         = 1.0f;
    float offset        = 0.0f;
    int32_t color_mode  = 0;
    int32_t channel     = 0;
    int32_t use_ocio    = 0;
    int32_t orientation = 1;
};

struct OcioVulkanTexture {
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkSampler sampler     = VK_NULL_HANDLE;
    uint32_t binding      = 0;
    int width             = 0;
    int height            = 0;
    int depth             = 0;
};

struct OcioVulkanState {
    OcioShaderRuntime* runtime                  = nullptr;
    VkDescriptorPool descriptor_pool            = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set              = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout            = VK_NULL_HANDLE;
    VkPipeline pipeline                         = VK_NULL_HANDLE;
    VkBuffer uniform_buffer                     = VK_NULL_HANDLE;
    VkDeviceMemory uniform_memory               = VK_NULL_HANDLE;
    void* uniform_mapped                        = nullptr;
    std::vector<OcioVulkanTexture> textures;
    size_t uniform_buffer_size = 0;
    std::string shader_cache_id;
    bool ready = false;
};

struct VulkanState {
    VkAllocationCallbacks* allocator                = nullptr;
    VkInstance instance                             = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger        = VK_NULL_HANDLE;
    uint32_t api_version                            = VK_API_VERSION_1_0;
    int framebuffer_width                           = 0;
    int framebuffer_height                          = 0;
    VkPhysicalDevice physical_device                = VK_NULL_HANDLE;
    VkDevice device                                 = VK_NULL_HANDLE;
    uint32_t queue_family                           = static_cast<uint32_t>(-1);
    VkQueueFamilyProperties queue_family_properties = {};
    VkQueue queue                                   = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache                  = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool                = VK_NULL_HANDLE;
    VkSurfaceKHR surface                            = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Window window_data;
    uint32_t min_image_count                            = 2;
    bool swapchain_rebuild                              = false;
    bool validation_layer_enabled                       = false;
    bool debug_utils_enabled                            = false;
    bool verbose_logging                                = false;
    bool verbose_validation_output                      = false;
    bool log_imgui_texture_updates                      = false;
    bool queue_requires_full_image_copies               = false;
    bool warned_about_full_imgui_uploads                = false;
    bool compute_upload_ready                           = false;
    bool compute_supports_float64                       = false;
    VkFormat compute_output_format                      = VK_FORMAT_UNDEFINED;
    VkDescriptorPool compute_descriptor_pool            = VK_NULL_HANDLE;
    VkDescriptorSetLayout compute_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout compute_pipeline_layout            = VK_NULL_HANDLE;
    VkPipeline compute_pipeline                         = VK_NULL_HANDLE;
    VkPipeline compute_pipeline_fp64                    = VK_NULL_HANDLE;

    VkDescriptorPool preview_descriptor_pool            = VK_NULL_HANDLE;
    VkDescriptorSetLayout preview_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout preview_pipeline_layout            = VK_NULL_HANDLE;
    VkPipeline preview_pipeline                         = VK_NULL_HANDLE;
    VkRenderPass preview_render_pass                    = VK_NULL_HANDLE;
    OcioVulkanState ocio;
    VkCommandPool immediate_command_pool                      = VK_NULL_HANDLE;
    VkCommandBuffer immediate_command_buffer                  = VK_NULL_HANDLE;
    VkFence immediate_submit_fence                            = VK_NULL_HANDLE;
    PFN_vkSetDebugUtilsObjectNameEXT set_debug_object_name_fn = nullptr;
    uint32_t max_image_dimension_2d                           = 0;
};

void
check_vk_result(VkResult err);
bool
find_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits,
                 VkMemoryPropertyFlags required, uint32_t& memory_type_index);
bool
find_memory_type_with_fallback(VkPhysicalDevice physical_device,
                               uint32_t type_bits,
                               VkMemoryPropertyFlags preferred,
                               uint32_t& memory_type_index);

template<typename HandleT>
inline uint64_t
vk_handle_to_u64(HandleT handle)
{
    if constexpr (std::is_pointer<HandleT>::value) {
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
    } else {
        return static_cast<uint64_t>(handle);
    }
}

template<typename HandleT>
inline void
set_vk_object_name(VulkanState& vk_state, VkObjectType object_type,
                   HandleT handle, const char* name)
{
    if (vk_state.set_debug_object_name_fn == nullptr || handle == VK_NULL_HANDLE
        || name == nullptr || name[0] == '\0') {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType   = object_type;
    info.objectHandle = vk_handle_to_u64(handle);
    info.pObjectName  = name;
    vk_state.set_debug_object_name_fn(vk_state.device, &info);
}

void
destroy_texture(VulkanState& vk_state, VulkanTexture& texture);
bool
create_texture(VulkanState& vk_state, const LoadedImage& image,
               VulkanTexture& texture, std::string& error_message);
bool
preview_controls_equal(const PreviewControls& a, const PreviewControls& b);
bool
quiesce_texture_preview_submission(VulkanState& vk_state,
                                   VulkanTexture& texture,
                                   std::string& error_message);
bool
update_preview_texture(VulkanState& vk_state, VulkanTexture& texture,
                       const LoadedImage* image,
                       const PlaceholderUiState& ui_state,
                       const PreviewControls& controls,
                       std::string& error_message);
bool
ensure_ocio_preview_resources(VulkanState& vk_state, VulkanTexture& texture,
                              const LoadedImage* image,
                              const PlaceholderUiState& ui_state,
                              const PreviewControls& controls,
                              std::string& error_message);
void
destroy_ocio_preview_resources(VulkanState& vk_state);
void
name_window_frame_objects(VulkanState& vk_state);
bool
setup_vulkan_instance(VulkanState& vk_state,
                      ImVector<const char*>& instance_extensions,
                      std::string& error_message);
bool
setup_vulkan_device(VulkanState& vk_state, std::string& error_message);
bool
setup_vulkan_window(VulkanState& vk_state, int width, int height,
                    std::string& error_message);
void
destroy_vulkan_surface(VulkanState& vk_state);
void
cleanup_vulkan_window(VulkanState& vk_state);
void
cleanup_vulkan(VulkanState& vk_state);
bool
imiv_vulkan_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                           unsigned int* pixels, void* user_data);
void
apply_imgui_texture_update_workarounds(VulkanState& vk_state,
                                       ImDrawData* draw_data);
void
destroy_immediate_submit_resources(VulkanState& vk_state);
bool
begin_immediate_submit(VulkanState& vk_state, VkCommandBuffer& out_command,
                       std::string& error_message);
bool
end_immediate_submit(VulkanState& vk_state, VkCommandBuffer command_buffer,
                     std::string& error_message);
void
frame_render(VulkanState& vk_state, ImDrawData* draw_data);
void
frame_present(VulkanState& vk_state);
bool
capture_swapchain_region_rgba8(VulkanState& vk_state, int x, int y, int w,
                               int h, unsigned int* pixels);

#endif

}  // namespace Imiv
