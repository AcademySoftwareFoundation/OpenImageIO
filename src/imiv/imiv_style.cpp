// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_style.h"

#include <imgui.h>

namespace Imiv {

namespace {

    ImVec4 lerp_color(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }

    ImVec4 scale_color_alpha(const ImVec4& c, float alpha)
    {
        return ImVec4(c.x, c.y, c.z, c.w * alpha);
    }

}  // namespace

AppStylePreset
sanitize_app_style_preset(int preset_value)
{
    if (preset_value < static_cast<int>(AppStylePreset::IvLight)
        || preset_value > static_cast<int>(AppStylePreset::ImGuiClassic)) {
        return AppStylePreset::ImGuiDark;
    }
    return static_cast<AppStylePreset>(preset_value);
}

const char*
app_style_preset_name(AppStylePreset preset)
{
    switch (preset) {
    case AppStylePreset::IvLight: return "IV Light";
    case AppStylePreset::IvDark: return "IV Dark";
    case AppStylePreset::ImGuiLight: return "ImGui Light";
    case AppStylePreset::ImGuiDark: return "ImGui Dark";
    case AppStylePreset::ImGuiClassic: return "ImGui Classic";
    }
    return "ImGui Dark";
}

void
apply_imgui_style_defaults()
{
    ImGuiStyle& style      = ImGui::GetStyle();
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.PopupBorderSize  = 0.0f;
    style.FrameBorderSize  = 0.0f;
}

void
apply_imgui_color_style(AppStylePreset preset)
{
    switch (preset) {
    case AppStylePreset::IvLight: StyleColorsIvLight(); break;
    case AppStylePreset::IvDark: StyleColorsIvDark(); break;
    case AppStylePreset::ImGuiLight: ImGui::StyleColorsLight(); break;
    case AppStylePreset::ImGuiDark: ImGui::StyleColorsDark(); break;
    case AppStylePreset::ImGuiClassic: ImGui::StyleColorsClassic(); break;
    }
}

void
apply_imgui_app_style(AppStylePreset preset)
{
    apply_imgui_color_style(preset);
    apply_imgui_style_defaults();
}

void
StyleColorsIvDark()
{
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors    = style->Colors;

    colors[ImGuiCol_Text]                 = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]               = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.48f, 0.29f, 0.16f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.98f, 0.59f, 0.26f, 0.40f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.48f, 0.29f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.98f, 0.44f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.88f, 0.39f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.98f, 0.44f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.98f, 0.44f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.98f, 0.44f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.98f, 0.39f, 0.06f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.98f, 0.44f, 0.26f, 0.31f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.98f, 0.44f, 0.26f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.98f, 0.44f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator]            = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.75f, 0.30f, 0.10f, 0.78f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.75f, 0.30f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.98f, 0.44f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.98f, 0.44f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.98f, 0.44f, 0.26f, 0.95f);
    colors[ImGuiCol_InputTextCursor]      = colors[ImGuiCol_Text];
    colors[ImGuiCol_TabHovered]           = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab]                  = lerp_color(colors[ImGuiCol_Header],
                                                       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabSelected] = lerp_color(colors[ImGuiCol_HeaderActive],
                                              colors[ImGuiCol_TitleBgActive],
                                              0.60f);
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TabDimmed]           = lerp_color(colors[ImGuiCol_Tab],
                                                      colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabDimmedSelected]
        = lerp_color(colors[ImGuiCol_TabSelected], colors[ImGuiCol_TitleBg],
                     0.40f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f,
                                                        0.00f);
    colors[ImGuiCol_DockingPreview]
        = scale_color_alpha(colors[ImGuiCol_HeaderActive], 0.7f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]              = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_TreeLines]             = colors[ImGuiCol_Border];
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_DragDropTargetBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_UnsavedMarker]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_NavCursor]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void
StyleColorsIvLight()
{
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors    = style->Colors;

    colors[ImGuiCol_Text]                 = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]             = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.98f, 0.59f, 0.26f, 0.40f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.98f, 0.59f, 0.26f, 0.78f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.80f, 0.54f, 0.46f, 0.60f);
    colors[ImGuiCol_Button]               = ImVec4(0.98f, 0.59f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.98f, 0.53f, 0.06f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.98f, 0.59f, 0.26f, 0.31f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.98f, 0.59f, 0.26f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.98f, 0.59f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.39f, 0.39f, 0.39f, 0.62f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.80f, 0.44f, 0.14f, 0.78f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.80f, 0.44f, 0.14f, 1.00f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.98f, 0.59f, 0.26f, 0.95f);
    colors[ImGuiCol_InputTextCursor]      = colors[ImGuiCol_Text];
    colors[ImGuiCol_TabHovered]           = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab]                  = lerp_color(colors[ImGuiCol_Header],
                                                       colors[ImGuiCol_TitleBgActive], 0.90f);
    colors[ImGuiCol_TabSelected] = lerp_color(colors[ImGuiCol_HeaderActive],
                                              colors[ImGuiCol_TitleBgActive],
                                              0.60f);
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TabDimmed]           = lerp_color(colors[ImGuiCol_Tab],
                                                      colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabDimmedSelected]
        = lerp_color(colors[ImGuiCol_TabSelected], colors[ImGuiCol_TitleBg],
                     0.40f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.26f, 0.59f, 1.00f,
                                                        0.00f);
    colors[ImGuiCol_DockingPreview] = scale_color_alpha(colors[ImGuiCol_Header],
                                                        0.7f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]      = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextLink]              = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_TreeLines]             = colors[ImGuiCol_Border];
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_DragDropTargetBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_UnsavedMarker]         = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavCursor]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

}  // namespace Imiv
