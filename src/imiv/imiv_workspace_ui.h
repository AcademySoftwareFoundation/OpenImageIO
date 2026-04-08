// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_workspace.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

void
ensure_image_list_default_layout(MultiViewWorkspace& workspace,
                                 ImGuiID dockspace_id);
void
reset_window_layouts(MultiViewWorkspace& workspace,
                     PlaceholderUiState& ui_state, ImGuiID dockspace_id);

}  // namespace Imiv
