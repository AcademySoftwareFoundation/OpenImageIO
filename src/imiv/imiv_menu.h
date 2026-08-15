// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_frame_actions.h"

namespace Imiv {

struct DeveloperUiState;

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
                      bool& show_image_list_window,
                      bool& image_list_request_focus
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                      ,
                      bool show_test_menu, bool* show_test_engine_windows
#endif
);

}  // namespace Imiv
