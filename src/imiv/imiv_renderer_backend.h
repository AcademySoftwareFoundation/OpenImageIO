// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"

namespace Imiv {

bool
renderer_backend_get_viewer_texture_refs(const ViewerState& viewer,
                                         const PlaceholderUiState& ui_state,
                                         ImTextureRef& main_texture_ref,
                                         bool& has_main_texture,
                                         ImTextureRef& closeup_texture_ref,
                                         bool& has_closeup_texture);

bool
renderer_backend_create_texture(RendererState& renderer_state,
                                const LoadedImage& image,
                                RendererTexture& texture,
                                std::string& error_message);
void
renderer_backend_destroy_texture(RendererState& renderer_state,
                                 RendererTexture& texture);
bool
renderer_backend_update_preview_texture(RendererState& renderer_state,
                                        RendererTexture& texture,
                                        const LoadedImage* image,
                                        const PlaceholderUiState& ui_state,
                                        const RendererPreviewControls& controls,
                                        std::string& error_message);
bool
renderer_backend_quiesce_texture_preview_submission(
    RendererState& renderer_state, RendererTexture& texture,
    std::string& error_message);

bool
renderer_backend_setup_instance(RendererState& renderer_state,
                                ImVector<const char*>& instance_extensions,
                                std::string& error_message);
bool
renderer_backend_setup_device(RendererState& renderer_state,
                              std::string& error_message);
bool
renderer_backend_setup_window(RendererState& renderer_state, int width,
                              int height, std::string& error_message);
bool
renderer_backend_create_surface(RendererState& renderer_state,
                                GLFWwindow* window,
                                std::string& error_message);
void
renderer_backend_destroy_surface(RendererState& renderer_state);
void
renderer_backend_cleanup_window(RendererState& renderer_state);
void
renderer_backend_cleanup(RendererState& renderer_state);
bool
renderer_backend_wait_idle(RendererState& renderer_state,
                           std::string& error_message);
bool
renderer_backend_imgui_init(RendererState& renderer_state,
                            std::string& error_message);
void
renderer_backend_imgui_shutdown();
void
renderer_backend_imgui_new_frame(RendererState& renderer_state);
bool
renderer_backend_needs_main_window_resize(RendererState& renderer_state,
                                          int width, int height);
void
renderer_backend_resize_main_window(RendererState& renderer_state, int width,
                                    int height);
void
renderer_backend_set_main_clear_color(RendererState& renderer_state, float r,
                                      float g, float b, float a);
void
renderer_backend_prepare_platform_windows(RendererState& renderer_state);
void
renderer_backend_finish_platform_windows(RendererState& renderer_state);
void
renderer_backend_frame_render(RendererState& renderer_state,
                              ImDrawData* draw_data);
void
renderer_backend_frame_present(RendererState& renderer_state);
bool
renderer_backend_screen_capture(ImGuiID viewport_id, int x, int y, int w,
                                int h, unsigned int* pixels, void* user_data);

}  // namespace Imiv
