// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"

namespace Imiv {

struct RendererBackendVTable {
    BackendKind kind = BackendKind::Auto;
    bool (*get_viewer_texture_refs)(const ViewerState& viewer,
                                    const PlaceholderUiState& ui_state,
                                    ImTextureRef& main_texture_ref,
                                    bool& has_main_texture,
                                    ImTextureRef& closeup_texture_ref,
                                    bool& has_closeup_texture) = nullptr;
    bool (*texture_is_loading)(const RendererTexture& texture) = nullptr;
    bool (*create_texture)(RendererState& renderer_state,
                           const LoadedImage& image,
                           RendererTexture& texture,
                           std::string& error_message) = nullptr;
    void (*destroy_texture)(RendererState& renderer_state,
                            RendererTexture& texture) = nullptr;
    bool (*update_preview_texture)(RendererState& renderer_state,
                                   RendererTexture& texture,
                                   const LoadedImage* image,
                                   const PlaceholderUiState& ui_state,
                                   const RendererPreviewControls& controls,
                                   std::string& error_message) = nullptr;
    bool (*quiesce_texture_preview_submission)(RendererState& renderer_state,
                                               RendererTexture& texture,
                                               std::string& error_message)
        = nullptr;
    bool (*setup_instance)(RendererState& renderer_state,
                           ImVector<const char*>& instance_extensions,
                           std::string& error_message) = nullptr;
    bool (*setup_device)(RendererState& renderer_state,
                         std::string& error_message) = nullptr;
    bool (*setup_window)(RendererState& renderer_state, int width, int height,
                         std::string& error_message) = nullptr;
    bool (*create_surface)(RendererState& renderer_state, GLFWwindow* window,
                           std::string& error_message) = nullptr;
    void (*destroy_surface)(RendererState& renderer_state) = nullptr;
    void (*cleanup_window)(RendererState& renderer_state) = nullptr;
    void (*cleanup)(RendererState& renderer_state) = nullptr;
    bool (*wait_idle)(RendererState& renderer_state,
                      std::string& error_message) = nullptr;
    bool (*imgui_init)(RendererState& renderer_state,
                       std::string& error_message) = nullptr;
    void (*imgui_shutdown)() = nullptr;
    void (*imgui_new_frame)(RendererState& renderer_state) = nullptr;
    bool (*needs_main_window_resize)(RendererState& renderer_state, int width,
                                     int height) = nullptr;
    void (*resize_main_window)(RendererState& renderer_state, int width,
                               int height) = nullptr;
    void (*set_main_clear_color)(RendererState& renderer_state, float r,
                                 float g, float b, float a) = nullptr;
    void (*prepare_platform_windows)(RendererState& renderer_state) = nullptr;
    void (*finish_platform_windows)(RendererState& renderer_state) = nullptr;
    void (*frame_render)(RendererState& renderer_state,
                         ImDrawData* draw_data) = nullptr;
    void (*frame_present)(RendererState& renderer_state) = nullptr;
    bool (*screen_capture)(ImGuiID viewport_id, int x, int y, int w, int h,
                           unsigned int* pixels, void* user_data) = nullptr;
};

#if IMIV_WITH_VULKAN
const RendererBackendVTable*
renderer_backend_vulkan_vtable();
#endif

#if IMIV_WITH_METAL
const RendererBackendVTable*
renderer_backend_metal_vtable();
#endif

#if IMIV_WITH_OPENGL
const RendererBackendVTable*
renderer_backend_opengl_vtable();
#endif

}  // namespace Imiv
