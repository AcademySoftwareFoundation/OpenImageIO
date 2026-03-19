// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_backend.h"
#include "imiv_types.h"

#include <imgui.h>

#include <string>

struct GLFWwindow;

namespace Imiv {

struct ViewerState;
struct PlaceholderUiState;
struct RendererBackendVTable;

struct RendererBackendState;
struct RendererTextureBackendState;

struct RendererState {
    const RendererBackendVTable* vtable = nullptr;
    RendererBackendState* backend        = nullptr;
    BackendKind active_backend           = BackendKind::Auto;
    bool verbose_logging                 = false;
    bool verbose_validation_output       = false;
    bool log_imgui_texture_updates       = false;
    float clear_color[4]                 = { 0.08f, 0.08f, 0.08f, 1.0f };
    int framebuffer_width                = 0;
    int framebuffer_height               = 0;
};

struct RendererTexture {
    const RendererBackendVTable* vtable = nullptr;
    BackendKind backend_kind            = BackendKind::Auto;
    RendererTextureBackendState* backend = nullptr;
    bool preview_initialized             = false;
};

struct RendererPreviewControls {
    float exposure           = 0.0f;
    float gamma              = 1.0f;
    float offset             = 0.0f;
    int color_mode           = 0;
    int channel              = 0;
    int use_ocio             = 0;
    int orientation          = 1;
    int linear_interpolation = 0;
};

void
renderer_select_backend(RendererState& renderer_state, BackendKind backend);
BackendKind
renderer_active_backend(const RendererState& renderer_state);
bool
renderer_texture_is_loading(const RendererTexture& texture);

void
renderer_get_viewer_texture_refs(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ImTextureRef& main_texture_ref,
                                 bool& has_main_texture,
                                 ImTextureRef& closeup_texture_ref,
                                 bool& has_closeup_texture);

bool
renderer_create_texture(RendererState& renderer_state, const LoadedImage& image,
                        RendererTexture& texture, std::string& error_message);
void
renderer_destroy_texture(RendererState& renderer_state,
                         RendererTexture& texture);
bool
renderer_update_preview_texture(RendererState& renderer_state,
                                RendererTexture& texture,
                                const LoadedImage* image,
                                const PlaceholderUiState& ui_state,
                                const RendererPreviewControls& controls,
                                std::string& error_message);
bool
renderer_quiesce_texture_preview_submission(RendererState& renderer_state,
                                            RendererTexture& texture,
                                            std::string& error_message);

bool
renderer_setup_instance(RendererState& renderer_state,
                        ImVector<const char*>& instance_extensions,
                        std::string& error_message);
bool
renderer_setup_device(RendererState& renderer_state,
                      std::string& error_message);
bool
renderer_setup_window(RendererState& renderer_state, int width, int height,
                      std::string& error_message);
bool
renderer_create_surface(RendererState& renderer_state, GLFWwindow* window,
                        std::string& error_message);
void
renderer_destroy_surface(RendererState& renderer_state);
void
renderer_cleanup_window(RendererState& renderer_state);
void
renderer_cleanup(RendererState& renderer_state);
bool
renderer_wait_idle(RendererState& renderer_state, std::string& error_message);
bool
renderer_imgui_init(RendererState& renderer_state, std::string& error_message);
void
renderer_imgui_shutdown(RendererState& renderer_state);
void
renderer_imgui_new_frame(RendererState& renderer_state);
bool
renderer_needs_main_window_resize(RendererState& renderer_state, int width,
                                  int height);
void
renderer_resize_main_window(RendererState& renderer_state, int width,
                            int height);
void
renderer_set_main_clear_color(RendererState& renderer_state, float r, float g,
                              float b, float a);
void
renderer_prepare_platform_windows(RendererState& renderer_state);
void
renderer_finish_platform_windows(RendererState& renderer_state);
void
renderer_frame_render(RendererState& renderer_state, ImDrawData* draw_data);
void
renderer_frame_present(RendererState& renderer_state);
bool
renderer_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                        unsigned int* pixels, void* user_data);

}  // namespace Imiv
