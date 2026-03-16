// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer.h"

#include "imiv_viewer.h"

namespace Imiv {

void
renderer_get_viewer_texture_refs(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ImTextureRef& main_texture_ref,
                                 bool& has_main_texture,
                                 ImTextureRef& closeup_texture_ref,
                                 bool& has_closeup_texture)
{
    main_texture_ref    = ImTextureRef();
    closeup_texture_ref = ImTextureRef();
    has_main_texture    = false;
    has_closeup_texture = false;

#if defined(IMIV_BACKEND_VULKAN_GLFW)
    if (!viewer.texture.preview_initialized)
        return;

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
#else
    (void)viewer;
    (void)ui_state;
#endif
}

bool
renderer_create_texture(RendererState& renderer_state, const LoadedImage& image,
                        RendererTexture& texture, std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return create_texture(renderer_state, image, texture, error_message);
#else
    (void)renderer_state;
    (void)image;
    (void)texture;
    error_message = "renderer backend is not implemented";
    return false;
#endif
}

void
renderer_destroy_texture(RendererState& renderer_state,
                         RendererTexture& texture)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    destroy_texture(renderer_state, texture);
#else
    (void)renderer_state;
    (void)texture;
#endif
}

bool
renderer_update_preview_texture(RendererState& renderer_state,
                                RendererTexture& texture,
                                const LoadedImage* image,
                                const PlaceholderUiState& ui_state,
                                const RendererPreviewControls& controls,
                                std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return update_preview_texture(renderer_state, texture, image, ui_state,
                                  controls, error_message);
#else
    (void)renderer_state;
    (void)texture;
    (void)image;
    (void)ui_state;
    (void)controls;
    error_message = "renderer backend is not implemented";
    return false;
#endif
}

bool
renderer_quiesce_texture_preview_submission(RendererState& renderer_state,
                                            RendererTexture& texture,
                                            std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return quiesce_texture_preview_submission(renderer_state, texture,
                                              error_message);
#else
    (void)renderer_state;
    (void)texture;
    error_message.clear();
    return true;
#endif
}

bool
renderer_setup_instance(RendererState& renderer_state,
                        ImVector<const char*>& instance_extensions,
                        std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return setup_vulkan_instance(renderer_state, instance_extensions,
                                 error_message);
#else
    (void)renderer_state;
    (void)instance_extensions;
    error_message = "renderer backend is not implemented";
    return false;
#endif
}

bool
renderer_setup_device(RendererState& renderer_state, std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return setup_vulkan_device(renderer_state, error_message);
#else
    (void)renderer_state;
    error_message = "renderer backend is not implemented";
    return false;
#endif
}

bool
renderer_setup_window(RendererState& renderer_state, int width, int height,
                      std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return setup_vulkan_window(renderer_state, width, height, error_message);
#else
    (void)renderer_state;
    (void)width;
    (void)height;
    error_message = "renderer backend is not implemented";
    return false;
#endif
}

void
renderer_destroy_surface(RendererState& renderer_state)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    destroy_vulkan_surface(renderer_state);
#else
    (void)renderer_state;
#endif
}

void
renderer_cleanup_window(RendererState& renderer_state)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    cleanup_vulkan_window(renderer_state);
#else
    (void)renderer_state;
#endif
}

void
renderer_cleanup(RendererState& renderer_state)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    cleanup_vulkan(renderer_state);
#else
    (void)renderer_state;
#endif
}

bool
renderer_wait_idle(RendererState& renderer_state, std::string& error_message)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
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
#else
    (void)renderer_state;
    error_message.clear();
    return true;
#endif
}

void
renderer_frame_render(RendererState& renderer_state, ImDrawData* draw_data)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    frame_render(renderer_state, draw_data);
#else
    (void)renderer_state;
    (void)draw_data;
#endif
}

void
renderer_frame_present(RendererState& renderer_state)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    frame_present(renderer_state);
#else
    (void)renderer_state;
#endif
}

bool
renderer_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                        unsigned int* pixels, void* user_data)
{
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    return imiv_vulkan_screen_capture(viewport_id, x, y, w, h, pixels,
                                      user_data);
#else
    (void)viewport_id;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)pixels;
    (void)user_data;
    return false;
#endif
}

}  // namespace Imiv
