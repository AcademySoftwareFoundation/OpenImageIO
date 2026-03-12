// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_file_dialog.h"
#include "imiv_ocio.h"
#include "imiv_test_engine.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    void set_aux_window_defaults(const ImVec2& offset, const ImVec2& size)
    {
        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImVec2 base_pos(0.0f, 0.0f);
        if (main_viewport != nullptr)
            base_pos = main_viewport->WorkPos;
        ImGui::SetNextWindowPos(ImVec2(base_pos.x + offset.x,
                                       base_pos.y + offset.y),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    }

    void push_preview_active_button_style(bool active)
    {
        if (!active)
            return;
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 112, 171, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              IM_COL32(80, 133, 200, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              IM_COL32(57, 95, 146, 255));
    }

    void pop_preview_active_button_style(bool active)
    {
        if (!active)
            return;
        ImGui::PopStyleColor(3);
    }

    void preview_form_next_row(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    bool draw_preview_row_button_cell(const char* label, bool active)
    {
        ImGui::TableNextColumn();
        push_preview_active_button_style(active);
        const bool pressed
            = ImGui::Button(label,
                            ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
        pop_preview_active_button_style(active);
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

    void draw_info_table_row(const char* label, const std::string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopTextWrapPos();
    }

    bool input_text_string(const char* label, std::string& value)
    {
        char buffer[4096];
        const size_t copy_size = std::min(value.size(), sizeof(buffer) - 1u);
        std::memcpy(buffer, value.data(), copy_size);
        buffer[copy_size]  = '\0';
        const bool changed = ImGui::InputText(label, buffer, sizeof(buffer));
        if (changed)
            value.assign(buffer);
        return changed;
    }

    std::string ocio_config_dialog_default_path(const PlaceholderUiState& ui)
    {
        if (!ui.ocio_user_config_path.empty()) {
            const std::filesystem::path user_path(ui.ocio_user_config_path);
            if (user_path.has_parent_path())
                return user_path.parent_path().string();
        }

        OcioConfigSelection selection;
        resolve_ocio_config_selection(ui, selection);
        if (!selection.resolved_path.empty()) {
            const std::filesystem::path resolved_path(selection.resolved_path);
            if (resolved_path.has_parent_path())
                return resolved_path.parent_path().string();
        }
        return std::string();
    }

}  // namespace

void
draw_info_window(const ViewerState& viewer, bool& show_window)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(72.0f, 72.0f), ImVec2(640.0f, 420.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin("iv Info", &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height  = std::max(100.0f,
                                            ImGui::GetContentRegionAvail().y
                                                - close_height - 4.0f);
        ImGui::BeginChild("##iv_info_scroll", ImVec2(0.0f, body_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (viewer.image.path.empty()) {
            draw_padded_message("No image loaded.", 8.0f, 8.0f);
            register_layout_dump_synthetic_item("text", "No image loaded.");
        } else {
            if (ImGui::BeginTable("##iv_info_table", 2,
                                  ImGuiTableFlags_SizingStretchProp
                                      | ImGuiTableFlags_BordersInnerV
                                      | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Field",
                                        ImGuiTableColumnFlags_WidthFixed,
                                        190.0f);
                ImGui::TableSetupColumn("Value",
                                        ImGuiTableColumnFlags_WidthStretch);

                draw_info_table_row("Path", viewer.image.path);
                for (const std::pair<std::string, std::string>& row :
                     viewer.image.longinfo_rows) {
                    draw_info_table_row(row.first.c_str(), row.second);
                }
                draw_info_table_row(
                    "Orientation",
                    Strutil::fmt::format("{}", viewer.image.orientation));
                draw_info_table_row(
                    "Subimage",
                    Strutil::fmt::format("{}/{}", viewer.image.subimage + 1,
                                         viewer.image.nsubimages));
                draw_info_table_row(
                    "MIP level",
                    Strutil::fmt::format("{}/{}", viewer.image.miplevel + 1,
                                         viewer.image.nmiplevels));
                draw_info_table_row(
                    "Row pitch (bytes)",
                    Strutil::fmt::format("{}", viewer.image.row_pitch_bytes));
                ImGui::EndTable();
            }
            register_layout_dump_synthetic_item("text", "iv Info content");
        }
        ImGui::EndChild();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        if (ImGui::Button("Close"))
            show_window = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void
draw_preferences_window(PlaceholderUiState& ui, bool& show_window)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(740.0f, 72.0f), ImVec2(520.0f, 360.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin("iv Preferences", &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height  = std::max(120.0f,
                                            ImGui::GetContentRegionAvail().y
                                                - close_height - 4.0f);
        ImGui::BeginChild("##iv_prefs_body", ImVec2(0.0f, body_height), false,
                          ImGuiWindowFlags_None);

        ImGui::Checkbox("Pixel view follows mouse",
                        &ui.pixelview_follows_mouse);
        register_layout_dump_synthetic_item("text", "Pixel view follows mouse");

        ImGui::Spacing();
        ImGui::TextUnformatted("# closeup pixels");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(76.0f);
        ImGui::InputInt("##pref_closeup_pixels", &ui.closeup_pixels, 2, 2);

        ImGui::TextUnformatted("# closeup avg pixels");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(76.0f);
        ImGui::InputInt("##pref_closeup_avg_pixels", &ui.closeup_avg_pixels, 2,
                        2);

        ImGui::Spacing();
        ImGui::Checkbox("Linear interpolation", &ui.linear_interpolation);
        ImGui::Checkbox("Dark palette", &ui.dark_palette);
        ImGui::Checkbox("Generate mipmaps (requires restart)", &ui.auto_mipmap);

        ImGui::Spacing();
        ImGui::TextUnformatted("Image Cache max memory (requires restart)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("##pref_max_mem", &ui.max_memory_ic_mb);
        ImGui::SameLine();
        ImGui::TextUnformatted("MB");

        ImGui::TextUnformatted("Slide Show delay");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("##pref_slide_delay", &ui.slide_duration_seconds);
        ImGui::SameLine();
        ImGui::TextUnformatted("s");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("OCIO config");
        int ocio_source = ui.ocio_config_source;
        if (ImGui::RadioButton("Global##ocio_cfg_global", &ocio_source,
                               static_cast<int>(OcioConfigSource::Global))) {
            ui.ocio_config_source = ocio_source;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Local##ocio_cfg_local", &ocio_source,
                               static_cast<int>(OcioConfigSource::Local))) {
            ui.ocio_config_source = ocio_source;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("User##ocio_cfg_user", &ocio_source,
                               static_cast<int>(OcioConfigSource::User))) {
            ui.ocio_config_source = ocio_source;
        }

        ImGui::TextUnformatted("User path");
        ImGui::SetNextItemWidth(
            std::max(80.0f, ImGui::GetContentRegionAvail().x - 84.0f));
        input_text_string("##pref_ocio_user_config_path",
                          ui.ocio_user_config_path);
        ImGui::SameLine();
        if (ImGui::Button("Browse##pref_ocio_user_config")) {
            const FileDialog::DialogReply reply
                = FileDialog::open_ocio_config_file(
                    ocio_config_dialog_default_path(ui));
            if (reply.result == FileDialog::Result::Okay
                && !reply.path.empty()) {
                ui.ocio_user_config_path = reply.path;
                ui.ocio_config_source    = static_cast<int>(
                    OcioConfigSource::User);
            }
        }

        OcioConfigSelection ocio_selection;
        resolve_ocio_config_selection(ui, ocio_selection);
        ImGui::TextUnformatted("Resolved source");
        ImGui::SameLine();
        ImGui::TextUnformatted(
            ocio_config_source_name(ocio_selection.resolved_source));
        if (ocio_selection.resolved_source == OcioConfigSource::Global) {
            const char* suffix = ocio_selection.uses_raw_fallback
                                     ? "($OCIO unset, OCIO raw fallback)"
                                     : "($OCIO)";
            ImGui::SameLine();
            ImGui::TextUnformatted(suffix);
        }
        if (!ocio_selection.resolved_path.empty()) {
            ImGui::TextUnformatted("Resolved path");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(ocio_selection.resolved_path.c_str());
            ImGui::PopTextWrapPos();
        } else if (ocio_selection.resolved_source == OcioConfigSource::Global
                   && !ocio_selection.env_value.empty()) {
            ImGui::TextUnformatted("OCIO env");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(ocio_selection.env_value.c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::EndChild();
        clamp_placeholder_ui_state(ui);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        if (ImGui::Button("Close"))
            show_window = false;
        register_layout_dump_synthetic_item("text", "iv Preferences content");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void
draw_preview_window(PlaceholderUiState& ui, bool& show_window)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(1030.0f, 72.0f), ImVec2(500.0f, 360.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin("iv Preview", &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height  = std::max(120.0f,
                                            ImGui::GetContentRegionAvail().y
                                                - close_height - 4.0f);
        ImGui::BeginChild("##iv_preview_body", ImVec2(0.0f, body_height), false,
                          ImGuiWindowFlags_NoScrollbar);

        if (ImGui::BeginTable("##iv_preview_form", 2,
                              ImGuiTableFlags_SizingStretchProp
                                  | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                    90.0f);
            ImGui::TableSetupColumn("Control",
                                    ImGuiTableColumnFlags_WidthStretch);

            preview_form_next_row("Interpolation");
            ImGui::Checkbox("Linear##preview_interp", &ui.linear_interpolation);

            preview_form_next_row("Exposure");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_exposure", &ui.exposure, -10.0f,
                               10.0f, "%.2f");

            preview_form_next_row("");
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

            preview_form_next_row("Gamma");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_gamma", &ui.gamma, 0.1f, 4.0f,
                               "%.2f");

            preview_form_next_row("");
            if (ImGui::BeginTable("##preview_gamma_steps", 2,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                if (draw_preview_row_button_cell("-0.1", false))
                    ui.gamma = std::max(0.1f, ui.gamma - 0.1f);
                if (draw_preview_row_button_cell("+0.1", false))
                    ui.gamma += 0.1f;
                ImGui::EndTable();
            }

            preview_form_next_row("Offset");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##preview_offset", &ui.offset, -1.0f, 1.0f,
                               "%+.3f");

            preview_form_next_row("");
            if (ImGui::Button("Reset",
                              ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                preview_reset_adjustments(ui);
            }

            preview_form_next_row("");
            if (ImGui::BeginTable("##preview_modes", 7,
                                  ImGuiTableFlags_SizingStretchSame
                                      | ImGuiTableFlags_NoSavedSettings)) {
                const bool rgb_active = ui.current_channel == 0
                                        && (ui.color_mode == 0
                                            || ui.color_mode == 1);
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
                if (draw_preview_row_button_cell("RGB", rgb_active))
                    preview_set_rgb_mode(ui);
                if (draw_preview_row_button_cell("Luma",
                                                 ui.color_mode == 3
                                                     && ui.current_channel
                                                            == 0)) {
                    preview_set_luma_mode(ui);
                }
                if (draw_preview_row_button_cell("R", red_active))
                    preview_set_single_channel_mode(ui, 1);
                if (draw_preview_row_button_cell("G", green_active))
                    preview_set_single_channel_mode(ui, 2);
                if (draw_preview_row_button_cell("B", blue_active))
                    preview_set_single_channel_mode(ui, 3);
                if (draw_preview_row_button_cell("A", alpha_active))
                    preview_set_single_channel_mode(ui, 4);
                if (draw_preview_row_button_cell("Heat", ui.color_mode == 4))
                    preview_set_heat_mode(ui);
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
