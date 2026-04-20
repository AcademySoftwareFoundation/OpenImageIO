// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_backend.h"
#include "imiv_renderer.h"
#include "imiv_ui.h"

#include <filesystem>
#include <string>

namespace Imiv {

struct DeveloperUiState {
    bool enabled                     = false;
    bool show_imgui_demo_window      = false;
    bool show_imgui_style_editor     = false;
    bool show_imgui_metrics_window   = false;
    bool show_imgui_debug_log_window = false;
    bool show_imgui_id_stack_window  = false;
    bool show_imgui_about_window     = false;
    bool request_screenshot          = false;
    bool screenshot_busy             = false;
    double screenshot_due_time       = -1.0;
};

void
apply_test_engine_ocio_overrides(PlaceholderUiState& ui_state);
void
apply_test_engine_view_activation_override(MultiViewWorkspace& workspace);
void
apply_test_engine_view_recipe_overrides(PlaceholderUiState& ui_state);
void
apply_test_engine_drop_overrides(ViewerState& viewer);
void
begin_developer_screenshot_request(DeveloperUiState& developer_ui,
                                   ViewerState& viewer);
void
draw_developer_windows(DeveloperUiState& developer_ui);

#if defined(IMGUI_ENABLE_TEST_ENGINE)
struct ViewerStateJsonWriteContext {
    const ViewerState* viewer           = nullptr;
    const MultiViewWorkspace* workspace = nullptr;
    const PlaceholderUiState* ui_state  = nullptr;
    BackendKind active_backend          = BackendKind::Auto;
};

bool
write_test_engine_viewer_state_json(const std::filesystem::path& out_path,
                                    void* user_data,
                                    std::string& error_message);
#endif

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
void
process_developer_post_render_actions(DeveloperUiState& developer_ui,
                                      ViewerState& viewer,
                                      RendererState& renderer_state);
#endif

}  // namespace Imiv
