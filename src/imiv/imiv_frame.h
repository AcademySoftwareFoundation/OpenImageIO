// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"
#include "imiv_ui.h"

#include <filesystem>
#include <string>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

struct DeveloperUiState {
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

#if defined(IMGUI_ENABLE_TEST_ENGINE)
struct ViewerStateJsonWriteContext {
    const ViewerState* viewer          = nullptr;
    const PlaceholderUiState* ui_state = nullptr;
};

bool
write_test_engine_viewer_state_json(const std::filesystem::path& out_path,
                                    void* user_data,
                                    std::string& error_message);
#endif

const char*
image_window_title();

void
draw_viewer_ui(ViewerState& viewer, PlaceholderUiState& ui_state,
               DeveloperUiState& developer_ui, const AppFonts& fonts,
               bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
               ,
               bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW)
               ,
               GLFWwindow* window, RendererState& renderer_state
#endif
);

#if defined(IMIV_BACKEND_VULKAN_GLFW)
void
process_developer_post_render_actions(DeveloperUiState& developer_ui,
                                      ViewerState& viewer,
                                      RendererState& renderer_state);
#endif

}  // namespace Imiv
