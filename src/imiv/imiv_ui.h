// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_backend.h"
#include "imiv_navigation.h"
#include "imiv_viewer.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

inline constexpr const char* k_info_window_title        = "iv Info";
inline constexpr const char* k_preferences_window_title = "iv Preferences";
inline constexpr const char* k_preview_window_title     = "iv Preview";
inline constexpr const char* k_about_window_title       = "About imiv";

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
bool
input_text_string(const char* label, std::string& value);
void
push_active_button_style(bool active);
void
pop_active_button_style(bool active);
bool
begin_two_column_table(const char* id, float label_column_width,
                       ImGuiTableFlags flags,
                       const char* label_column_name = "Label",
                       const char* value_column_name = "Value");
void
table_labeled_row(const char* label);
void
draw_wrapped_value_row(const char* label, const char* value);
void
draw_section_heading(const char* title, float separator_padding_y);
void
align_control_right(float width);
bool
draw_right_aligned_checkbox(const char* id, bool& value);
void
draw_right_aligned_text(const char* value);
bool
draw_right_aligned_int_stepper(const char* id, int& value, int step,
                               const char* suffix, float button_width,
                               float value_width);
void
draw_disabled_wrapped_text(const char* message);
void
draw_info_window(const ViewerState& viewer, bool& show_window,
                 bool reset_layout = false);
void
draw_preferences_window(PlaceholderUiState& ui, bool& show_window,
                        BackendKind active_backend, bool reset_layout = false);
void
draw_preview_window(PlaceholderUiState& ui, bool& show_window,
                    bool reset_layout = false);
void
draw_image_selection_overlay(const ViewerState& viewer,
                             const ImageCoordinateMap& map);
void
draw_embedded_status_bar(ViewerState& viewer, PlaceholderUiState& ui);

}  // namespace Imiv
