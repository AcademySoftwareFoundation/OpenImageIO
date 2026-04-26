// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"
#include "imiv_vulkan_types.h"

#if IMIV_WITH_VULKAN

#    include "imiv_platform_glfw.h"
#    include "imiv_viewer.h"

#    include <imgui_impl_vulkan.h>

#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>

namespace Imiv {
namespace {

    bool ensure_backend_state(RendererState& renderer_state)
    {
        if (renderer_state.backend != nullptr)
            return true;
        VulkanState* vk_state = new VulkanState();
        if (vk_state == nullptr)
            return false;
        vk_state->verbose_logging = renderer_state.verbose_logging;
        vk_state->verbose_validation_output
            = renderer_state.verbose_validation_output;
        vk_state->log_imgui_texture_updates
            = renderer_state.log_imgui_texture_updates;
        vk_state->requested_display_format
            = renderer_state.requested_display_format;
        renderer_state.backend = reinterpret_cast<RendererBackendState*>(
            vk_state);
        return renderer_state.backend != nullptr;
    }

    VulkanState* ensure_vulkan_backend_state(RendererState& renderer_state,
                                             std::string& error_message)
    {
        if (!ensure_backend_state(renderer_state)) {
            error_message = "failed to allocate Vulkan renderer state";
            return nullptr;
        }
        return backend_state<VulkanState>(renderer_state);
    }

    VulkanState* vulkan_backend_state(RendererState& renderer_state,
                                      std::string& error_message)
    {
        VulkanState* vk_state = backend_state<VulkanState>(renderer_state);
        if (vk_state == nullptr)
            error_message = "Vulkan renderer state is unavailable";
        return vk_state;
    }

    const VulkanState* vulkan_backend_state(const RendererState& renderer_state)
    {
        return backend_state<VulkanState>(renderer_state);
    }

    int vulkan_display_color_bits(VkFormat format)
    {
        switch (format) {
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SRGB: return 8;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return 10;
        default: break;
        }
        return 0;
    }

    DisplayDynamicRange vulkan_display_dynamic_range(VkColorSpaceKHR color_space)
    {
        switch (color_space) {
        case VK_COLORSPACE_SRGB_NONLINEAR_KHR: return DisplayDynamicRange::Sdr;
        default: break;
        }
        return DisplayDynamicRange::Unknown;
    }

    void update_vulkan_display_presentation(RendererState& renderer_state,
                                            const VulkanState& vk_state)
    {
        DisplayPresentationInfo info;
        info.color_bits = vulkan_display_color_bits(
            vk_state.window_data.SurfaceFormat.format);
        info.range = vulkan_display_dynamic_range(
            vk_state.window_data.SurfaceFormat.colorSpace);
        info.format_request_fell_back = renderer_state.requested_display_format
                                            == DisplayFormatPreference::Rgb10A2
                                        && (info.color_bits < 10
                                            || info.range
                                                   != DisplayDynamicRange::Sdr);
        renderer_state.display_presentation = info;
    }

    bool vulkan_get_viewer_texture_refs(const ViewerState& viewer,
                                        const PlaceholderUiState& ui_state,
                                        ImTextureRef& main_texture_ref,
                                        bool& has_main_texture,
                                        ImTextureRef& closeup_texture_ref,
                                        bool& has_closeup_texture)
    {
        const VulkanTexture* texture = texture_backend_state<VulkanTexture>(
            viewer.texture);
        if (texture == nullptr || !viewer.texture.preview_initialized)
            return false;

        VkDescriptorSet main_set = ui_state.linear_interpolation
                                       ? texture->set
                                       : texture->nearest_mag_set;
        if (main_set == VK_NULL_HANDLE)
            main_set = texture->set;
        if (main_set != VK_NULL_HANDLE) {
            main_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                reinterpret_cast<uintptr_t>(main_set)));
            has_main_texture = true;
        }

        if (texture->pixelview_set != VK_NULL_HANDLE) {
            closeup_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                reinterpret_cast<uintptr_t>(texture->pixelview_set)));
            has_closeup_texture = true;
        } else if (has_main_texture) {
            closeup_texture_ref = main_texture_ref;
            has_closeup_texture = true;
        }
        return has_main_texture || has_closeup_texture;
    }

    bool vulkan_texture_is_loading(const RendererTexture& texture)
    {
        const VulkanTexture* vk_texture = texture_backend_state<VulkanTexture>(
            texture);
        if (vk_texture == nullptr)
            return false;
        return vk_texture->upload_submit_pending
               || vk_texture->preview_submit_pending
               || (!texture.preview_initialized
                   && vk_texture->set != VK_NULL_HANDLE);
    }

    bool vulkan_create_texture(RendererState& renderer_state,
                               const LoadedImage& image,
                               RendererTexture& texture,
                               std::string& error_message)
    {
        VulkanState* vk_state = ensure_vulkan_backend_state(renderer_state,
                                                            error_message);
        if (vk_state == nullptr)
            return false;

        VulkanTexture vk_texture;
        if (!create_texture(*vk_state, image, vk_texture, error_message))
            return false;

        texture.backend = reinterpret_cast<RendererTextureBackendState*>(
            new VulkanTexture(std::move(vk_texture)));
        if (texture.backend == nullptr) {
            error_message = "failed to allocate Vulkan texture state";
            return false;
        }
        return true;
    }

    void vulkan_destroy_texture(RendererState& renderer_state,
                                RendererTexture& texture)
    {
        VulkanState* vk_state     = backend_state<VulkanState>(renderer_state);
        VulkanTexture* vk_texture = texture_backend_state<VulkanTexture>(
            texture);
        if (vk_state != nullptr && vk_texture != nullptr)
            retire_texture(*vk_state, *vk_texture);
        delete vk_texture;
    }

    bool vulkan_update_preview_texture(RendererState& renderer_state,
                                       RendererTexture& texture,
                                       const LoadedImage* image,
                                       const PlaceholderUiState& ui_state,
                                       const PreviewControls& controls,
                                       std::string& error_message)
    {
        VulkanState* vk_state     = vulkan_backend_state(renderer_state,
                                                         error_message);
        VulkanTexture* vk_texture = texture_backend_state<VulkanTexture>(
            texture);
        if (vk_state == nullptr || vk_texture == nullptr) {
            if (vk_texture == nullptr)
                error_message = "Vulkan renderer state is unavailable";
            return false;
        }
        const bool ok = update_preview_texture(*vk_state, *vk_texture, image,
                                               ui_state, controls,
                                               error_message);
        texture.preview_initialized = vk_texture->preview_initialized;
        return ok;
    }

    bool vulkan_quiesce_texture_preview_submission(RendererState& renderer_state,
                                                   RendererTexture& texture,
                                                   std::string& error_message)
    {
        VulkanState* vk_state     = backend_state<VulkanState>(renderer_state);
        VulkanTexture* vk_texture = texture_backend_state<VulkanTexture>(
            texture);
        if (vk_state == nullptr || vk_texture == nullptr) {
            error_message.clear();
            return true;
        }
        return quiesce_texture_preview_submission(*vk_state, *vk_texture,
                                                  error_message);
    }

    bool vulkan_setup_instance(RendererState& renderer_state,
                               ImVector<const char*>& instance_extensions,
                               std::string& error_message)
    {
        VulkanState* vk_state = ensure_vulkan_backend_state(renderer_state,
                                                            error_message);
        return vk_state != nullptr
               && setup_vulkan_instance(*vk_state, instance_extensions,
                                        error_message);
    }

    bool vulkan_wait_idle(RendererState& renderer_state,
                          std::string& error_message)
    {
        VulkanState* vk_state = backend_state<VulkanState>(renderer_state);
        if (vk_state == nullptr || vk_state->device == VK_NULL_HANDLE) {
            error_message.clear();
            return true;
        }
        const VkResult err = vkDeviceWaitIdle(vk_state->device);
        if (err == VK_SUCCESS) {
            drain_retired_textures(*vk_state, true);
            error_message.clear();
            return true;
        }
        check_vk_result(err);
        error_message = "renderer wait idle failed";
        return false;
    }

    bool vulkan_imgui_init(RendererState& renderer_state,
                           std::string& error_message)
    {
        VulkanState* vk_state = vulkan_backend_state(renderer_state,
                                                     error_message);
        if (vk_state == nullptr)
            return false;

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.ApiVersion                = vk_state->api_version;
        init_info.Instance                  = vk_state->instance;
        init_info.PhysicalDevice            = vk_state->physical_device;
        init_info.Device                    = vk_state->device;
        init_info.QueueFamily               = vk_state->queue_family;
        init_info.Queue                     = vk_state->queue;
        init_info.PipelineCache             = vk_state->pipeline_cache;
        init_info.DescriptorPool            = vk_state->descriptor_pool;
        init_info.MinImageCount             = vk_state->min_image_count;
        init_info.ImageCount                = vk_state->window_data.ImageCount;
        init_info.Allocator                 = vk_state->allocator;
        init_info.PipelineInfoMain.RenderPass = vk_state->window_data.RenderPass;
        init_info.PipelineInfoMain.Subpass     = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn              = check_vk_result;
        if (ImGui_ImplVulkan_Init(&init_info)) {
            error_message.clear();
            return true;
        }
        error_message = "ImGui_ImplVulkan_Init failed";
        return false;
    }

    bool vulkan_needs_main_window_resize(RendererState& renderer_state,
                                         int width, int height)
    {
        const VulkanState* vk_state = vulkan_backend_state(renderer_state);
        if (vk_state == nullptr)
            return false;
        return vk_state->swapchain_rebuild
               || vk_state->window_data.Width != width
               || vk_state->window_data.Height != height;
    }

    void vulkan_resize_main_window(RendererState& renderer_state, int width,
                                   int height)
    {
        VulkanState* vk_state = backend_state<VulkanState>(renderer_state);
        if (vk_state == nullptr)
            return;
        renderer_set_framebuffer_size(renderer_state, width, height);
        ImGui_ImplVulkan_SetMinImageCount(vk_state->min_image_count);
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            vk_state->instance, vk_state->physical_device, vk_state->device,
            &vk_state->window_data, vk_state->queue_family, vk_state->allocator,
            width, height, vk_state->min_image_count,
            vk_state->window_image_usage);
        name_window_frame_objects(*vk_state);
        update_vulkan_display_presentation(renderer_state, *vk_state);
        vk_state->window_data.FrameIndex = 0;
        vk_state->swapchain_rebuild      = false;
    }

    void vulkan_set_main_clear_color(RendererState& renderer_state, float r,
                                     float g, float b, float a)
    {
        VulkanState* vk_state = backend_state<VulkanState>(renderer_state);
        if (vk_state == nullptr)
            return;
        vk_state->window_data.ClearValue.color.float32[0] = r;
        vk_state->window_data.ClearValue.color.float32[1] = g;
        vk_state->window_data.ClearValue.color.float32[2] = b;
        vk_state->window_data.ClearValue.color.float32[3] = a;
    }

    const RendererBackendVTable k_vulkan_vtable = {
        BackendKind::Vulkan,
        platform_glfw_supports_vulkan,
        vulkan_get_viewer_texture_refs,
        vulkan_texture_is_loading,
        vulkan_create_texture,
        vulkan_destroy_texture,
        vulkan_update_preview_texture,
        vulkan_quiesce_texture_preview_submission,
        vulkan_setup_instance,
        [](RendererState& renderer_state, std::string& error_message) {
            VulkanState* vk_state = vulkan_backend_state(renderer_state,
                                                         error_message);
            return vk_state != nullptr
                   && setup_vulkan_device(*vk_state, error_message);
        },
        [](RendererState& renderer_state, int width, int height,
           std::string& error_message) {
            VulkanState* vk_state = vulkan_backend_state(renderer_state,
                                                         error_message);
            if (vk_state == nullptr)
                return false;
            renderer_set_framebuffer_size(renderer_state, width, height);
            if (!setup_vulkan_window(*vk_state, width, height, error_message))
                return false;
            update_vulkan_display_presentation(renderer_state, *vk_state);
            return true;
        },
        [](RendererState& renderer_state, GLFWwindow* window,
           std::string& error_message) {
            VulkanState* vk_state = vulkan_backend_state(renderer_state,
                                                         error_message);
            if (vk_state == nullptr)
                return false;
            const VkResult err = glfwCreateWindowSurface(vk_state->instance,
                                                         window,
                                                         vk_state->allocator,
                                                         &vk_state->surface);
            if (err == VK_SUCCESS) {
                error_message.clear();
                return true;
            }
            check_vk_result(err);
            error_message = "glfwCreateWindowSurface failed";
            return false;
        },
        [](RendererState& renderer_state) {
            if (VulkanState* vk_state = backend_state<VulkanState>(
                    renderer_state)) {
                destroy_vulkan_surface(*vk_state);
            }
        },
        [](RendererState& renderer_state) {
            if (VulkanState* vk_state = backend_state<VulkanState>(
                    renderer_state)) {
                cleanup_vulkan_window(*vk_state);
            }
        },
        [](RendererState& renderer_state) {
            VulkanState* vk_state = backend_state<VulkanState>(renderer_state);
            if (vk_state != nullptr)
                cleanup_vulkan(*vk_state);
            delete vk_state;
            renderer_state.backend = nullptr;
        },
        vulkan_wait_idle,
        vulkan_imgui_init,
        ImGui_ImplVulkan_Shutdown,
        renderer_call_backend_new_frame<ImGui_ImplVulkan_NewFrame>,
        vulkan_needs_main_window_resize,
        vulkan_resize_main_window,
        vulkan_set_main_clear_color,
        renderer_noop_platform_windows,
        renderer_noop_platform_windows,
        [](RendererState& renderer_state, ImDrawData* draw_data) {
            if (VulkanState* vk_state = backend_state<VulkanState>(
                    renderer_state)) {
                frame_render(*vk_state, draw_data);
            }
        },
        [](RendererState& renderer_state) {
            if (VulkanState* vk_state = backend_state<VulkanState>(
                    renderer_state)) {
                frame_present(*vk_state);
            }
        },
        imiv_vulkan_screen_capture,
    };

}  // namespace

const RendererBackendVTable*
renderer_backend_vulkan_vtable()
{
    return &k_vulkan_vtable;
}

}  // namespace Imiv

#endif
