// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_test_engine.h"
#include "imiv_ui_metrics.h"

#include <algorithm>
#include <string>
#include <utility>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    bool draw_preview_row_button_cell(const char* label, bool active)
    {
        ImGui::TableNextColumn();
        push_active_button_style(active);
        const bool pressed
            = ImGui::Button(label,
                            ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
        pop_active_button_style(active);
        return pressed;
    }

    void preview_set_rgb_mode(PlaceholderUiState& ui)
    {
        ui.color_mode      = 1;
        ui.current_channel = 0;
    }

    void preview_set_luma_mode(PlaceholderUiState& ui)
    {
        ui.color_mode      = 3;
        ui.current_channel = 0;
    }

    void preview_set_single_channel_mode(PlaceholderUiState& ui, int channel)
    {
        ui.color_mode      = 2;
        ui.current_channel = channel;
    }

    void preview_set_heat_mode(PlaceholderUiState& ui)
    {
        ui.color_mode = 4;
        if (ui.current_channel <= 0)
            ui.current_channel = 1;
    }

    void preview_reset_adjustments(PlaceholderUiState& ui)
    {
        ui.exposure = 0.0f;
        ui.gamma    = 1.0f;
        ui.offset   = 0.0f;
    }

}  // namespace

void
draw_info_window(const ViewerState& viewer, bool& show_window,
                 bool reset_layout)
{
    if (!show_window)
        return;
    set_aux_window_defaults(UiMetrics::AuxiliaryWindows::kInfoOffset,
                            UiMetrics::AuxiliaryWindows::kInfoSize,
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        UiMetrics::kAuxWindowPadding);
    if (ImGui::Begin(k_info_window_title, &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height
            = std::max(UiMetrics::AuxiliaryWindows::kInfoBodyMinHeight,
                       ImGui::GetContentRegionAvail().y - close_height
                           - UiMetrics::AuxiliaryWindows::kBodyBottomGap);
        ImGui::BeginChild("##iv_info_scroll", ImVec2(0.0f, body_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (viewer.image.path.empty()) {
            draw_padded_message(
                "No image loaded.",
                UiMetrics::AuxiliaryWindows::kEmptyMessagePadding.x,
                UiMetrics::AuxiliaryWindows::kEmptyMessagePadding.y);
            register_layout_dump_synthetic_item("text", "No image loaded.");
        } else {
            if (begin_two_column_table(
                    "##iv_info_table",
                    UiMetrics::AuxiliaryWindows::kInfoTableLabelWidth,
                    ImGuiTableFlags_SizingStretchProp
                        | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg,
                    "Field", "Value")) {
                draw_wrapped_value_row("Path", viewer.image.path.c_str());
                for (const std::pair<std::string, std::string>& row :
                     viewer.image.longinfo_rows) {
                    draw_wrapped_value_row(row.first.c_str(),
                                           row.second.c_str());
                }
                draw_wrapped_value_row(
                    "Orientation",
                    Strutil::fmt::format("{}", viewer.image.orientation)
                        .c_str());
                draw_wrapped_value_row(
                    "Subimage",
                    Strutil::fmt::format("{}/{}", viewer.image.subimage + 1,
                                         viewer.image.nsubimages)
                        .c_str());
                draw_wrapped_value_row(
                    "MIP level",
                    Strutil::fmt::format("{}/{}", viewer.image.miplevel + 1,
                                         viewer.image.nmiplevels)
                        .c_str());
                draw_wrapped_value_row(
                    "Row pitch (bytes)",
                    Strutil::fmt::format("{}", viewer.image.row_pitch_bytes)
                        .c_str());
                ImGui::EndTable();
            }
            register_layout_dump_synthetic_item("text", "iv Info content");
        }
        ImGui::EndChild();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()
                             + UiMetrics::AuxiliaryWindows::kInfoCloseGap);
        if (ImGui::Button("Close"))
            show_window = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void
draw_preview_window(PlaceholderUiState& ui, bool& show_window,
                    bool reset_layout)
{
    if (!show_window)
        return;
    set_aux_window_defaults(UiMetrics::AuxiliaryWindows::kPreviewOffset,
                            UiMetrics::AuxiliaryWindows::kPreviewSize,
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        UiMetrics::kAuxWindowPadding);
    if (ImGui::Begin(k_preview_window_title, &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height
            = std::max(UiMetrics::AuxiliaryWindows::kPreviewBodyMinHeight,
                       ImGui::GetContentRegionAvail().y - close_height
                           - UiMetrics::AuxiliaryWindows::kBodyBottomGap);
        ImGui::BeginChild("##iv_preview_body", ImVec2(0.0f, body_height), false,
                          ImGuiWindowFlags_NoScrollbar);

        if (begin_two_column_table("##iv_preview_form",
                                   UiMetrics::Preview::kLabelColumnWidth,
                                   ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_NoSavedSettings)) {
            table_labeled_row("Interpolation");
            ImGui::Checkbox("Linear##preview_interp", &ui.linear_interpolation);

            table_labeled_row("Exposure");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_exposure", &ui.exposure, -10.0f,
                               10.0f, "%.2f");

            table_labeled_row("");
            if (ImGui::BeginTable("##preview_exposure_steps", 4,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                if (draw_preview_row_button_cell("-1/2", false))
                    ui.exposure -= 0.5f;
                if (draw_preview_row_button_cell("-1/10", false))
                    ui.exposure -= 0.1f;
                if (draw_preview_row_button_cell("+1/10", false))
                    ui.exposure += 0.1f;
                if (draw_preview_row_button_cell("+1/2", false))
                    ui.exposure += 0.5f;
                ImGui::EndTable();
            }

            table_labeled_row("Gamma");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_gamma", &ui.gamma, 0.1f, 4.0f,
                               "%.2f");

            table_labeled_row("");
            if (ImGui::BeginTable("##preview_gamma_steps", 2,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                if (draw_preview_row_button_cell("-0.1", false))
                    ui.gamma = std::max(0.1f, ui.gamma - 0.1f);
                if (draw_preview_row_button_cell("+0.1", false))
                    ui.gamma += 0.1f;
                ImGui::EndTable();
            }

            table_labeled_row("Offset");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_offset", &ui.offset, -1.0f, 1.0f,
                               "%+.3f");

            table_labeled_row("");
            if (ImGui::Button("Reset",
                              ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                preview_reset_adjustments(ui);
            }

            table_labeled_row("");
            if (ImGui::BeginTable("##preview_modes", 3,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                const bool rgb_active = ui.current_channel == 0
                                        && (ui.color_mode == 0
                                            || ui.color_mode == 1);
                if (draw_preview_row_button_cell("RGB", rgb_active))
                    preview_set_rgb_mode(ui);
                if (draw_preview_row_button_cell("Luma",
                                                 ui.color_mode == 3
                                                     && ui.current_channel
                                                            == 0)) {
                    preview_set_luma_mode(ui);
                }
                if (draw_preview_row_button_cell("Heat", ui.color_mode == 4))
                    preview_set_heat_mode(ui);
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##rgb_modes", 4,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                const bool red_active = ui.current_channel == 1
                                        && ui.color_mode != 3
                                        && ui.color_mode != 4;
                const bool green_active = ui.current_channel == 2
                                          && ui.color_mode != 3
                                          && ui.color_mode != 4;
                const bool blue_active = ui.current_channel == 3
                                         && ui.color_mode != 3
                                         && ui.color_mode != 4;
                const bool alpha_active = ui.current_channel == 4
                                          && ui.color_mode != 3
                                          && ui.color_mode != 4;
                if (draw_preview_row_button_cell("R", red_active))
                    preview_set_single_channel_mode(ui, 1);
                if (draw_preview_row_button_cell("G", green_active))
                    preview_set_single_channel_mode(ui, 2);
                if (draw_preview_row_button_cell("B", blue_active))
                    preview_set_single_channel_mode(ui, 3);
                if (draw_preview_row_button_cell("A", alpha_active))
                    preview_set_single_channel_mode(ui, 4);
                ImGui::EndTable();
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
        clamp_placeholder_ui_state(ui);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        if (ImGui::Button("Close"))
            show_window = false;
        register_layout_dump_synthetic_item("text", "iv Preview content");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

}  // namespace Imiv
