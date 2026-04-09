// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_ui.h"

#include <string>
#include <vector>

namespace Imiv {

struct ProbeOverlayText {
    std::vector<std::string> lines;
    std::vector<ImU32> colors;
};

void
reset_area_probe_overlay(ViewerState& viewer);
void
update_area_probe_overlay(ViewerState& viewer, int xbegin, int ybegin, int xend,
                          int yend);
void
sync_area_probe_to_selection(ViewerState& viewer,
                             const PlaceholderUiState& ui_state);
void
build_area_probe_overlay_lines(const ViewerState& viewer,
                               std::vector<std::string>& out_lines);
void
build_pixel_closeup_overlay_text(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ProbeOverlayText& out_text);
OverlayPanelRect
draw_pixel_closeup_overlay(const ViewerState& viewer,
                           PlaceholderUiState& ui_state,
                           const ImageCoordinateMap& map,
                           ImTextureRef closeup_texture,
                           bool has_closeup_texture, const AppFonts& fonts);
void
draw_area_probe_overlay(const ViewerState& viewer,
                        const PlaceholderUiState& ui_state,
                        const ImageCoordinateMap& map,
                        const OverlayPanelRect& pixel_overlay_panel,
                        const AppFonts& fonts);

}  // namespace Imiv
