// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_ui.h"

namespace Imiv {

void
draw_image_window_contents(ViewerState& viewer, PlaceholderUiState& ui_state,
                           const AppFonts& fonts,
                           const PendingZoomRequest& shortcut_zoom_request,
                           bool recenter_requested);

}  // namespace Imiv
