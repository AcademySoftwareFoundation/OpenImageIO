// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_backend.h"
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

    void set_aux_window_defaults(const ImVec2& offset, const ImVec2& size,
                                 bool reset_layout)
    {
        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImVec2 base_pos(0.0f, 0.0f);
        if (main_viewport != nullptr)
            base_pos = main_viewport->WorkPos;
        const ImGuiCond cond = reset_layout ? ImGuiCond_Always
                                            : ImGuiCond_FirstUseEver;
        ImGui::SetNextWindowPos(ImVec2(base_pos.x + offset.x,
                                       base_pos.y + offset.y),
                                cond);
        ImGui::SetNextWindowSize(size, cond);
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
        if ((selection.resolved_source == OcioConfigSource::User
             || selection.resolved_source == OcioConfigSource::Global)
            && !selection.resolved_path.empty()) {
            if (Strutil::istarts_with(selection.resolved_path, "ocio://"))
                return std::string();
            const std::filesystem::path resolved_path(selection.resolved_path);
            if (resolved_path.has_parent_path())
                return resolved_path.parent_path().string();
        }
        return std::string();
    }

    void draw_preferences_section_heading(const char* title)
    {
        const ImVec2 separator_padding
            = ImVec2(ImGui::GetStyle().SeparatorTextPadding.x, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding,
                            separator_padding);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::SeparatorText(title);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    bool begin_preferences_form_table(const char* id)
    {
        if (!ImGui::BeginTable(id, 2,
                               ImGuiTableFlags_SizingStretchProp
                                   | ImGuiTableFlags_NoSavedSettings)) {
            return false;
        }
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                150.0f);
        ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    void preferences_form_next_row(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    void align_preferences_control_right(float width)
    {
        const float available_width = ImGui::GetContentRegionAvail().x;
        if (available_width > width) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width
                                 - width);
        }
    }

    bool draw_preferences_right_aligned_checkbox(const char* id, bool& value)
    {
        align_preferences_control_right(ImGui::GetFrameHeight());
        return ImGui::Checkbox(id, &value);
    }

    void draw_preferences_right_aligned_text(const char* value)
    {
        const float width = ImGui::CalcTextSize(value).x;
        align_preferences_control_right(width);
        ImGui::TextUnformatted(value);
    }

    bool draw_preferences_right_aligned_int_stepper(const char* id, int& value,
                                                    int step,
                                                    const char* suffix)
    {
        ImGui::PushID(id);
        const float spacing      = ImGui::GetStyle().ItemSpacing.x;
        const float button_width = 22.0f;
        const float value_width  = 38.0f;
        const float suffix_width = (suffix != nullptr && suffix[0] != '\0')
                                       ? ImGui::CalcTextSize(suffix).x + spacing
                                       : 0.0f;
        const float total_width  = value_width + button_width * 2.0f
                                  + spacing * 2.0f + suffix_width;
        bool changed = false;
        align_preferences_control_right(total_width);
        ImGui::SetNextItemWidth(value_width);
        changed |= ImGui::InputInt("##value", &value, 0, 0);
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("-", ImVec2(button_width, 0.0f))) {
            value -= step;
            changed = true;
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("+", ImVec2(button_width, 0.0f))) {
            value += step;
            changed = true;
        }
        if (suffix != nullptr && suffix[0] != '\0') {
            ImGui::SameLine(0.0f, spacing);
            ImGui::TextUnformatted(suffix);
        }
        ImGui::PopID();
        return changed;
    }

    bool draw_preferences_segment_button(const char* id, const char* label,
                                         bool selected, bool enabled,
                                         float width)
    {
        ImGui::PushID(id);
        if (!enabled)
            ImGui::BeginDisabled();
        push_preview_active_button_style(selected);
        const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
        pop_preview_active_button_style(selected);
        if (!enabled)
            ImGui::EndDisabled();
        ImGui::PopID();
        return pressed && enabled;
    }

    void draw_preferences_note(const char* message)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(message);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

}  // namespace

void
draw_info_window(const ViewerState& viewer, bool& show_window,
                 bool reset_layout)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(72.0f, 72.0f), ImVec2(360.0f, 600.0f),
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin(k_info_window_title, &show_window)) {
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
                                        120.0f);
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
draw_preferences_window(PlaceholderUiState& ui, bool& show_window,
                        BackendKind active_backend, bool reset_layout)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(740.0f, 72.0f), ImVec2(300.0f, 700.0f),
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin(k_preferences_window_title, &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height  = std::max(120.0f,
                                            ImGui::GetContentRegionAvail().y
                                                - close_height - 4.0f);
        ImGui::BeginChild("##iv_prefs_body", ImVec2(0.0f, body_height), false,
                          ImGuiWindowFlags_None);

        const AppStylePreset current_style_preset = sanitize_app_style_preset(
            ui.style_preset);
        BackendKind requested_backend = sanitize_backend_kind(
            ui.renderer_backend);
        const float spacing        = ImGui::GetStyle().ItemSpacing.x;
        bool have_backend_row_rect = false;
        ImVec2 backend_row_min(0.0f, 0.0f);
        ImVec2 backend_row_max(0.0f, 0.0f);

        draw_preferences_section_heading("Theme");
        if (ImGui::BeginCombo("##pref_ui_style",
                              app_style_preset_name(current_style_preset))) {
            for (int preset_value = static_cast<int>(AppStylePreset::IvLight);
                 preset_value <= static_cast<int>(AppStylePreset::ImGuiClassic);
                 ++preset_value) {
                const AppStylePreset preset = static_cast<AppStylePreset>(
                    preset_value);
                const bool selected = preset == current_style_preset;
                if (ImGui::Selectable(app_style_preset_name(preset), selected)) {
                    ui.style_preset = preset_value;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        draw_preferences_section_heading("Pixel View");
        if (begin_preferences_form_table("##pref_pixel_view")) {
            preferences_form_next_row("Follows mouse");
            draw_preferences_right_aligned_checkbox(
                "##pref_pixelview_follows_mouse", ui.pixelview_follows_mouse);
            register_layout_dump_synthetic_item("text",
                                                "Pixel view follows mouse");

            preferences_form_next_row("Closeup pixels");
            draw_preferences_right_aligned_int_stepper("pref_closeup_pixels",
                                                       ui.closeup_pixels, 2,
                                                       nullptr);

            preferences_form_next_row("Closeup average");
            draw_preferences_right_aligned_int_stepper(
                "pref_closeup_avg_pixels", ui.closeup_avg_pixels, 2, nullptr);
            ImGui::EndTable();
        }

        draw_preferences_section_heading("Image Rendering");
        if (begin_preferences_form_table("##pref_image_rendering")) {
            preferences_form_next_row("Linear interpolation");
            draw_preferences_right_aligned_checkbox(
                "##pref_linear_interpolation", ui.linear_interpolation);
            ImGui::EndTable();
        }

        draw_preferences_section_heading("OCIO Config");
        {
            const float row_width = ImGui::GetContentRegionAvail().x;
            const float button_width
                = std::max(1.0f, (row_width - spacing * 2.0f) / 3.0f);
            const int ocio_source = ui.ocio_config_source;
            if (draw_preferences_segment_button(
                    "ocio_cfg_global", "Global",
                    ocio_source == static_cast<int>(OcioConfigSource::Global),
                    true, button_width)) {
                ui.ocio_config_source = static_cast<int>(
                    OcioConfigSource::Global);
            }
            ImGui::SameLine(0.0f, spacing);
            if (draw_preferences_segment_button(
                    "ocio_cfg_builtin", "Built-in",
                    ocio_source == static_cast<int>(OcioConfigSource::BuiltIn),
                    true, button_width)) {
                ui.ocio_config_source = static_cast<int>(
                    OcioConfigSource::BuiltIn);
            }
            ImGui::SameLine(0.0f, spacing);
            if (draw_preferences_segment_button(
                    "ocio_cfg_user", "User",
                    ocio_source == static_cast<int>(OcioConfigSource::User),
                    true, button_width)) {
                ui.ocio_config_source = static_cast<int>(
                    OcioConfigSource::User);
            }
        }
        if (static_cast<OcioConfigSource>(ui.ocio_config_source)
            == OcioConfigSource::User) {
            if (begin_preferences_form_table("##pref_ocio_user")) {
                preferences_form_next_row("Path");
                const float browse_width = 64.0f;
                const float field_width
                    = std::max(60.0f, ImGui::GetContentRegionAvail().x
                                          - browse_width - spacing);
                ImGui::SetNextItemWidth(field_width);
                input_text_string("##pref_ocio_user_config_path",
                                  ui.ocio_user_config_path);
                ImGui::SameLine(0.0f, spacing);
                if (ImGui::Button("Browse##pref_ocio_user_config",
                                  ImVec2(browse_width, 0.0f))) {
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
                ImGui::EndTable();
            }
        }
        OcioConfigSelection ocio_selection;
        resolve_ocio_config_selection(ui, ocio_selection);
        if (begin_preferences_form_table("##pref_ocio_info")) {
            preferences_form_next_row("Resolved source");
            draw_preferences_right_aligned_text(
                ocio_config_source_name(ocio_selection.resolved_source));

            if (!ocio_selection.resolved_path.empty()) {
                preferences_form_next_row("Resolved path");
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(ocio_selection.resolved_path.c_str());
                ImGui::PopTextWrapPos();
            }
            if (ocio_selection.resolved_source == OcioConfigSource::Global
                && !ocio_selection.env_value.empty()) {
                preferences_form_next_row("OCIO env");
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(ocio_selection.env_value.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::EndTable();
        }
        if (ocio_selection.requested_source == OcioConfigSource::Global
            && ocio_selection.resolved_source == OcioConfigSource::BuiltIn) {
            draw_preferences_note(
                "$OCIO is missing or invalid. Built-in config will be used.");
        } else if (ocio_selection.requested_source == OcioConfigSource::User
                   && ocio_selection.fallback_applied) {
            draw_preferences_note(
                "User config is missing or invalid. A fallback config will be used.");
        }

        draw_preferences_section_heading("System (required restart)");
        {
            const float row_width = std::max(1.0f,
                                             ImGui::GetContentRegionAvail().x);
            const float button_width
                = std::max(1.0f, (row_width - spacing * 3.0f) / 4.0f);

            const bool auto_selected = requested_backend == BackendKind::Auto;
            if (draw_preferences_segment_button("pref_backend_auto", "Auto",
                                                auto_selected, true,
                                                button_width)) {
                ui.renderer_backend = static_cast<int>(BackendKind::Auto);
            }
            register_test_engine_item_label("pref-backend:auto");
            register_layout_dump_synthetic_item("button",
                                                "Renderer backend Auto");

            const ImVec2 auto_item_min = ImGui::GetItemRectMin();
            const ImVec2 auto_item_max = ImGui::GetItemRectMax();
            backend_row_min            = auto_item_min;
            backend_row_max            = auto_item_max;
            have_backend_row_rect      = true;

            for (const BackendRuntimeInfo& info : runtime_backend_info()) {
                ImGui::SameLine(0.0f, spacing);
                const bool selected = requested_backend == info.build_info.kind;
                const bool enabled = info.build_info.compiled && info.available;
                if (draw_preferences_segment_button(
                        backend_cli_name(info.build_info.kind),
                        backend_display_name(info.build_info.kind), selected,
                        enabled, button_width)) {
                    ui.renderer_backend = static_cast<int>(
                        info.build_info.kind);
                }
                const std::string test_label = std::string("pref-backend:")
                                               + backend_cli_name(
                                                   info.build_info.kind);
                register_test_engine_item_label(test_label.c_str());
                register_layout_dump_synthetic_item(
                    "button", Strutil::fmt::format("Renderer backend {}",
                                                   backend_display_name(
                                                       info.build_info.kind))
                                  .c_str());
                const ImVec2 item_min = ImGui::GetItemRectMin();
                const ImVec2 item_max = ImGui::GetItemRectMax();
                backend_row_min.x     = std::min(backend_row_min.x, item_min.x);
                backend_row_min.y     = std::min(backend_row_min.y, item_min.y);
                backend_row_max.x     = std::max(backend_row_max.x, item_max.x);
                backend_row_max.y     = std::max(backend_row_max.y, item_max.y);
            }
            if (have_backend_row_rect) {
                register_layout_dump_synthetic_rect("button",
                                                    "Renderer backend",
                                                    backend_row_min,
                                                    backend_row_max);
            }
        }
        requested_backend = sanitize_backend_kind(ui.renderer_backend);
        const BackendKind next_launch_backend = resolve_backend_request(
            requested_backend);
        const bool requested_backend_compiled
            = requested_backend == BackendKind::Auto
              || backend_kind_is_compiled(requested_backend);
        const bool requested_backend_available
            = requested_backend == BackendKind::Auto
              || backend_kind_is_available(requested_backend);
        const bool invalid_requested_backend = requested_backend
                                                   != BackendKind::Auto
                                               && !requested_backend_compiled;
        const bool unavailable_requested_backend
            = requested_backend != BackendKind::Auto
              && requested_backend_compiled && !requested_backend_available;
        if (begin_preferences_form_table("##pref_system_info")) {
            const char* stored_preference
                = (requested_backend == BackendKind::Auto)
                      ? "Auto"
                      : backend_display_name(requested_backend);
            preferences_form_next_row("Stored preference");
            draw_preferences_right_aligned_text(stored_preference);

            preferences_form_next_row("Current backend");
            draw_preferences_right_aligned_text(
                backend_display_name(active_backend));

            preferences_form_next_row("Next launch backend");
            draw_preferences_right_aligned_text(
                backend_display_name(next_launch_backend));

            preferences_form_next_row("Generate mipmaps");
            draw_preferences_right_aligned_checkbox("##pref_auto_mipmap",
                                                    ui.auto_mipmap);
            ImGui::EndTable();
        }
        if (invalid_requested_backend) {
            draw_preferences_note(
                "Requested backend is not built in this binary and will be ignored when Preferences closes.");
        } else if (unavailable_requested_backend) {
            const std::string_view unavailable_reason
                = backend_unavailable_reason(requested_backend);
            if (unavailable_reason.empty()) {
                draw_preferences_note(
                    "Requested backend is unavailable at runtime and will be ignored when Preferences closes.");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_TextDisabled));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::Text(
                    "Requested backend is unavailable at runtime (%s) and will be ignored when Preferences closes.",
                    std::string(unavailable_reason).c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
        } else if (next_launch_backend != active_backend) {
            draw_preferences_note("Backend change requires restart.");
        }
        if (requested_backend == BackendKind::Auto)
            draw_preferences_note("Auto selects the first available backend.");
        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            if (!info.build_info.compiled || info.available)
                continue;
            if (info.unavailable_reason.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_TextDisabled));
                ImGui::Text("%s unavailable", info.build_info.display_name);
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_TextDisabled));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::Text("%s unavailable: %s", info.build_info.display_name,
                            info.unavailable_reason.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
        }

        draw_preferences_section_heading("Memory");
        if (begin_preferences_form_table("##pref_memory")) {
            preferences_form_next_row("Image cache max memory");
            draw_preferences_right_aligned_int_stepper("pref_max_mem",
                                                       ui.max_memory_ic_mb, 64,
                                                       "MB");
            ImGui::EndTable();
        }

        draw_preferences_section_heading("Slide Show");
        if (begin_preferences_form_table("##pref_slideshow")) {
            preferences_form_next_row("Delay");
            draw_preferences_right_aligned_int_stepper(
                "pref_slide_delay", ui.slide_duration_seconds, 1, "s");
            ImGui::EndTable();
        }

        ImGui::EndChild();
        clamp_placeholder_ui_state(ui);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        const float close_button_width = 72.0f;
        const float x                  = ImGui::GetCursorPosX();
        const float available_width    = ImGui::GetContentRegionAvail().x;
        if (available_width > close_button_width) {
            ImGui::SetCursorPosX(
                x + (available_width - close_button_width) * 0.5f);
        }
        if (ImGui::Button("Close", ImVec2(close_button_width, 0.0f)))
            show_window = false;
        register_layout_dump_synthetic_item("text", "iv Preferences content");
    }
    ImGui::End();
    ImGui::PopStyleVar();
    const BackendKind close_backend = sanitize_backend_kind(
        ui.renderer_backend);
    if (!show_window && close_backend != BackendKind::Auto
        && (!backend_kind_is_compiled(close_backend)
            || !backend_kind_is_available(close_backend))) {
        ui.renderer_backend = static_cast<int>(BackendKind::Auto);
    }
}

void
draw_preview_window(PlaceholderUiState& ui, bool& show_window,
                    bool reset_layout)
{
    if (!show_window)
        return;
    set_aux_window_defaults(ImVec2(1030.0f, 72.0f), ImVec2(300.0f, 360.0f),
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    if (ImGui::Begin(k_preview_window_title, &show_window)) {
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
