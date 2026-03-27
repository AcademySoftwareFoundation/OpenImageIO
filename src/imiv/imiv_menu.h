// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_navigation.h"
#include "imiv_renderer.h"
#include "imiv_types.h"
#include "imiv_viewer.h"

#include <string>

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

struct DeveloperUiState;

struct ViewerFrameActions {
    bool open_requested                = false;
    bool open_folder_requested         = false;
    bool save_as_requested             = false;
    bool clear_recent_requested        = false;
    bool reload_requested              = false;
    bool close_requested               = false;
    bool prev_requested                = false;
    bool next_requested                = false;
    bool toggle_requested              = false;
    bool prev_subimage_requested       = false;
    bool next_subimage_requested       = false;
    bool prev_mip_requested            = false;
    bool next_mip_requested            = false;
    bool save_window_as_requested      = false;
    bool save_selection_as_requested   = false;
    bool select_all_requested          = false;
    bool deselect_selection_requested  = false;
    bool fit_window_to_image_requested = false;
    bool recenter_requested            = false;
    bool new_view_requested            = false;
    bool delete_from_disk_requested    = false;
    bool full_screen_toggle_requested  = false;
    bool rotate_left_requested         = false;
    bool rotate_right_requested        = false;
    bool flip_horizontal_requested     = false;
    bool flip_vertical_requested       = false;
    PendingZoomRequest pending_zoom;
    std::string recent_open_path;
};

void
collect_viewer_shortcuts(ViewerState& viewer, PlaceholderUiState& ui_state,
                         DeveloperUiState& developer_ui,
                         ViewerFrameActions& actions, bool& request_exit);
void
draw_viewer_main_menu(ViewerState& viewer, PlaceholderUiState& ui_state,
                      ImageLibraryState& library,
                      const std::vector<ViewerState*>& viewers,
                      DeveloperUiState& developer_ui,
                      ViewerFrameActions& actions, bool& request_exit,
                      bool& show_image_list_window
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                      ,
                      bool show_test_menu, bool* show_test_engine_windows
#endif
);
void
execute_viewer_frame_actions(ViewerState& viewer, PlaceholderUiState& ui_state,
                             ImageLibraryState& library,
                             MultiViewWorkspace* workspace,
                             ViewerFrameActions& actions
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
                             ,
                             GLFWwindow* window, RendererState& renderer_state
#endif
);

}  // namespace Imiv
