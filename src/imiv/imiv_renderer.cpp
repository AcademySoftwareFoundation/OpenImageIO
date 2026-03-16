// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer.h"

#include "imiv_renderer_backend.h"
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
    renderer_backend_get_viewer_texture_refs(viewer, ui_state,
                                             main_texture_ref,
                                             has_main_texture,
                                             closeup_texture_ref,
                                             has_closeup_texture);
}

bool
renderer_create_texture(RendererState& renderer_state, const LoadedImage& image,
                        RendererTexture& texture, std::string& error_message)
{
    return renderer_backend_create_texture(renderer_state, image, texture,
                                           error_message);
}

void
renderer_destroy_texture(RendererState& renderer_state,
                         RendererTexture& texture)
{
    renderer_backend_destroy_texture(renderer_state, texture);
}

bool
renderer_update_preview_texture(RendererState& renderer_state,
                                RendererTexture& texture,
                                const LoadedImage* image,
                                const PlaceholderUiState& ui_state,
                                const RendererPreviewControls& controls,
                                std::string& error_message)
{
    return renderer_backend_update_preview_texture(renderer_state, texture,
                                                   image, ui_state, controls,
                                                   error_message);
}

bool
renderer_quiesce_texture_preview_submission(RendererState& renderer_state,
                                            RendererTexture& texture,
                                            std::string& error_message)
{
    return renderer_backend_quiesce_texture_preview_submission(
        renderer_state, texture, error_message);
}

bool
renderer_setup_instance(RendererState& renderer_state,
                        ImVector<const char*>& instance_extensions,
                        std::string& error_message)
{
    return renderer_backend_setup_instance(renderer_state, instance_extensions,
                                           error_message);
}

bool
renderer_setup_device(RendererState& renderer_state, std::string& error_message)
{
    return renderer_backend_setup_device(renderer_state, error_message);
}

bool
renderer_setup_window(RendererState& renderer_state, int width, int height,
                      std::string& error_message)
{
    return renderer_backend_setup_window(renderer_state, width, height,
                                         error_message);
}

bool
renderer_create_surface(RendererState& renderer_state, GLFWwindow* window,
                        std::string& error_message)
{
    return renderer_backend_create_surface(renderer_state, window,
                                           error_message);
}

void
renderer_destroy_surface(RendererState& renderer_state)
{
    renderer_backend_destroy_surface(renderer_state);
}

void
renderer_cleanup_window(RendererState& renderer_state)
{
    renderer_backend_cleanup_window(renderer_state);
}

void
renderer_cleanup(RendererState& renderer_state)
{
    renderer_backend_cleanup(renderer_state);
}

bool
renderer_wait_idle(RendererState& renderer_state, std::string& error_message)
{
    return renderer_backend_wait_idle(renderer_state, error_message);
}

bool
renderer_imgui_init(RendererState& renderer_state, std::string& error_message)
{
    return renderer_backend_imgui_init(renderer_state, error_message);
}

void
renderer_imgui_shutdown()
{
    renderer_backend_imgui_shutdown();
}

void
renderer_imgui_new_frame(RendererState& renderer_state)
{
    renderer_backend_imgui_new_frame(renderer_state);
}

bool
renderer_needs_main_window_resize(RendererState& renderer_state, int width,
                                  int height)
{
    return renderer_backend_needs_main_window_resize(renderer_state, width,
                                                     height);
}

void
renderer_resize_main_window(RendererState& renderer_state, int width,
                            int height)
{
    renderer_backend_resize_main_window(renderer_state, width, height);
}

void
renderer_set_main_clear_color(RendererState& renderer_state, float r, float g,
                              float b, float a)
{
    renderer_backend_set_main_clear_color(renderer_state, r, g, b, a);
}

void
renderer_prepare_platform_windows(RendererState& renderer_state)
{
    renderer_backend_prepare_platform_windows(renderer_state);
}

void
renderer_finish_platform_windows(RendererState& renderer_state)
{
    renderer_backend_finish_platform_windows(renderer_state);
}

void
renderer_frame_render(RendererState& renderer_state, ImDrawData* draw_data)
{
    renderer_backend_frame_render(renderer_state, draw_data);
}

void
renderer_frame_present(RendererState& renderer_state)
{
    renderer_backend_frame_present(renderer_state);
}

bool
renderer_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                        unsigned int* pixels, void* user_data)
{
    return renderer_backend_screen_capture(viewport_id, x, y, w, h, pixels,
                                           user_data);
}

}  // namespace Imiv
