// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

void
ensure_image_list_default_layout(MultiViewWorkspace& workspace,
                                 ImGuiID dockspace_id);
void
update_image_list_visibility_policy(MultiViewWorkspace& workspace,
                                    const ImageLibraryState& library);
void
apply_test_engine_image_list_visibility_override(MultiViewWorkspace& workspace);
std::vector<int>
image_list_open_view_ids(const MultiViewWorkspace& workspace,
                         const std::string& path);
void
draw_image_list_window(MultiViewWorkspace& workspace,
                       ImageLibraryState& library, ViewerState& active_view,
                       PlaceholderUiState& ui_state,
                       RendererState& renderer_state, bool reset_layout);
void
reset_window_layouts(MultiViewWorkspace& workspace,
                     PlaceholderUiState& ui_state, ImGuiID dockspace_id);

}  // namespace Imiv
