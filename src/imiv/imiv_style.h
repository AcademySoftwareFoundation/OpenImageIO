// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

namespace Imiv {

enum class AppStylePreset : int {
    IvLight      = 0,
    IvDark       = 1,
    ImGuiLight   = 2,
    ImGuiDark    = 3,
    ImGuiClassic = 4
};

AppStylePreset
sanitize_app_style_preset(int preset_value);
const char*
app_style_preset_name(AppStylePreset preset);
void
apply_imgui_style_defaults();
void
apply_imgui_color_style(AppStylePreset preset);
void
apply_imgui_app_style(AppStylePreset preset);
void
StyleColorsIvDark();
void
StyleColorsIvLight();

}  // namespace Imiv
