// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_viewer.h"

#include <imgui_impl_vulkan.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Imiv {

bool
renderer_backend_get_viewer_texture_refs(const ViewerState& viewer,
                                         const PlaceholderUiState& ui_state,
                                         ImTextureRef& main_texture_ref,
                                         bool& has_main_texture,
                                         ImTextureRef& closeup_texture_ref,
                                         bool& has_closeup_texture)
{
    if (!viewer.texture.preview_initialized)
        return false;

    VkDescriptorSet main_set = ui_state.linear_interpolation
                                   ? viewer.texture.set
                                   : viewer.texture.nearest_mag_set;
    if (main_set == VK_NULL_HANDLE)
        main_set = viewer.texture.set;
    if (main_set != VK_NULL_HANDLE) {
        main_texture_ref = ImTextureRef(
            static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(main_set)));
        has_main_texture = true;
    }

    if (viewer.texture.pixelview_set != VK_NULL_HANDLE) {
        closeup_texture_ref = ImTextureRef(static_cast<ImTextureID>(
            reinterpret_cast<uintptr_t>(viewer.texture.pixelview_set)));
        has_closeup_texture = true;
    } else if (has_main_texture) {
        closeup_texture_ref = main_texture_ref;
        has_closeup_texture = true;
    }
    return has_main_texture || has_closeup_texture;
}

bool
renderer_backend_create_texture(RendererState& renderer_state,
                                const LoadedImage& image,
                                RendererTexture& texture,
                                std::string& error_message)
{
    return create_texture(renderer_state, image, texture, error_message);
}

void
renderer_backend_destroy_texture(RendererState& renderer_state,
                                 RendererTexture& texture)
{
    destroy_texture(renderer_state, texture);
}

bool
renderer_backend_update_preview_texture(RendererState& renderer_state,
                                        RendererTexture& texture,
                                        const LoadedImage* image,
                                        const PlaceholderUiState& ui_state,
                                        const RendererPreviewControls& controls,
                                        std::string& error_message)
{
    return update_preview_texture(renderer_state, texture, image, ui_state,
                                  controls, error_message);
}

bool
renderer_backend_quiesce_texture_preview_submission(
    RendererState& renderer_state, RendererTexture& texture,
    std::string& error_message)
{
    return quiesce_texture_preview_submission(renderer_state, texture,
                                              error_message);
}

bool
renderer_backend_setup_instance(RendererState& renderer_state,
                                ImVector<const char*>& instance_extensions,
                                std::string& error_message)
{
    return setup_vulkan_instance(renderer_state, instance_extensions,
                                 error_message);
}

bool
renderer_backend_setup_device(RendererState& renderer_state,
                              std::string& error_message)
{
    return setup_vulkan_device(renderer_state, error_message);
}

bool
renderer_backend_setup_window(RendererState& renderer_state, int width,
                              int height, std::string& error_message)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    return setup_vulkan_window(renderer_state, width, height, error_message);
}

bool
renderer_backend_create_surface(RendererState& renderer_state,
                                GLFWwindow* window, std::string& error_message)
{
    const VkResult err = glfwCreateWindowSurface(renderer_state.instance,
                                                 window,
                                                 renderer_state.allocator,
                                                 &renderer_state.surface);
    if (err == VK_SUCCESS) {
        error_message.clear();
        return true;
    }
    check_vk_result(err);
    error_message = "glfwCreateWindowSurface failed";
    return false;
}

void
renderer_backend_destroy_surface(RendererState& renderer_state)
{
    destroy_vulkan_surface(renderer_state);
}

void
renderer_backend_cleanup_window(RendererState& renderer_state)
{
    cleanup_vulkan_window(renderer_state);
}

void
renderer_backend_cleanup(RendererState& renderer_state)
{
    cleanup_vulkan(renderer_state);
}

bool
renderer_backend_wait_idle(RendererState& renderer_state,
                           std::string& error_message)
{
    if (renderer_state.device == VK_NULL_HANDLE) {
        error_message.clear();
        return true;
    }
    const VkResult err = vkDeviceWaitIdle(renderer_state.device);
    if (err == VK_SUCCESS) {
        error_message.clear();
        return true;
    }
    check_vk_result(err);
    error_message = "renderer wait idle failed";
    return false;
}

bool
renderer_backend_imgui_init(RendererState& renderer_state,
                            std::string& error_message)
{
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion                = renderer_state.api_version;
    init_info.Instance                  = renderer_state.instance;
    init_info.PhysicalDevice            = renderer_state.physical_device;
    init_info.Device                    = renderer_state.device;
    init_info.QueueFamily               = renderer_state.queue_family;
    init_info.Queue                     = renderer_state.queue;
    init_info.PipelineCache             = renderer_state.pipeline_cache;
    init_info.DescriptorPool            = renderer_state.descriptor_pool;
    init_info.MinImageCount             = renderer_state.min_image_count;
    init_info.ImageCount                = renderer_state.window_data.ImageCount;
    init_info.Allocator                 = renderer_state.allocator;
    init_info.PipelineInfoMain.RenderPass
        = renderer_state.window_data.RenderPass;
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

void
renderer_backend_imgui_shutdown()
{
    ImGui_ImplVulkan_Shutdown();
}

void
renderer_backend_imgui_new_frame(RendererState& renderer_state)
{
    (void)renderer_state;
    ImGui_ImplVulkan_NewFrame();
}

bool
renderer_backend_needs_main_window_resize(RendererState& renderer_state,
                                          int width, int height)
{
    return renderer_state.swapchain_rebuild
           || renderer_state.window_data.Width != width
           || renderer_state.window_data.Height != height;
}

void
renderer_backend_resize_main_window(RendererState& renderer_state, int width,
                                    int height)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    ImGui_ImplVulkan_SetMinImageCount(renderer_state.min_image_count);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        renderer_state.instance, renderer_state.physical_device,
        renderer_state.device, &renderer_state.window_data,
        renderer_state.queue_family, renderer_state.allocator, width, height,
        renderer_state.min_image_count, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    name_window_frame_objects(renderer_state);
    renderer_state.window_data.FrameIndex = 0;
    renderer_state.swapchain_rebuild      = false;
}

void
renderer_backend_set_main_clear_color(RendererState& renderer_state, float r,
                                      float g, float b, float a)
{
    renderer_state.window_data.ClearValue.color.float32[0] = r;
    renderer_state.window_data.ClearValue.color.float32[1] = g;
    renderer_state.window_data.ClearValue.color.float32[2] = b;
    renderer_state.window_data.ClearValue.color.float32[3] = a;
}

void
renderer_backend_prepare_platform_windows(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_finish_platform_windows(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_frame_render(RendererState& renderer_state,
                              ImDrawData* draw_data)
{
    frame_render(renderer_state, draw_data);
}

void
renderer_backend_frame_present(RendererState& renderer_state)
{
    frame_present(renderer_state);
}

bool
renderer_backend_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                                unsigned int* pixels, void* user_data)
{
    return imiv_vulkan_screen_capture(viewport_id, x, y, w, h, pixels,
                                      user_data);
}

}  // namespace Imiv
