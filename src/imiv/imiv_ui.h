// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_navigation.h"
#include "imiv_viewer.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

struct AppFonts {
    ImFont* ui   = nullptr;
    ImFont* mono = nullptr;
};

struct OverlayPanelRect {
    bool valid = false;
    ImVec2 min = ImVec2(0.0f, 0.0f);
    ImVec2 max = ImVec2(0.0f, 0.0f);
};

bool
sample_loaded_pixel(const LoadedImage& image, int x, int y,
                    std::vector<double>& out_channels);
void
draw_padded_message(const char* message, float x_pad = 10.0f,
                    float y_pad = 6.0f);
void
draw_info_window(const ViewerState& viewer, bool& show_window);
void
draw_preferences_window(PlaceholderUiState& ui, bool& show_window);
void
draw_preview_window(PlaceholderUiState& ui, bool& show_window);
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
void
draw_embedded_status_bar(const ViewerState& viewer, PlaceholderUiState& ui);

}  // namespace Imiv
