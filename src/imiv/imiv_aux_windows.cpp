// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_backend.h"
#include "imiv_file_dialog.h"
#include "imiv_ocio.h"
#include "imiv_test_engine.h"
#include "imiv_ui_metrics.h"

#include <algorithm>
#include <filesystem>
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

    ImVec4 preview_theme_background_color(const PlaceholderUiState& ui)
    {
        return default_image_window_background_color(
            sanitize_app_style_preset(ui.style_preset));
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

        const ImGuiColorEditFlags color_flags
            = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB;
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transparency")) {
            ImGui::Checkbox("Show checkerboard", &ui.show_transparency);
            ImGui::SliderInt("Check size", &ui.transparency_check_size, 4, 128,
                             "%d px");
            ImGui::ColorEdit4("Light checks", &ui.transparency_light_color.x,
                              color_flags);
            ImGui::ColorEdit4("Dark checks", &ui.transparency_dark_color.x,
                              color_flags);

            ImVec4 theme_background_color = preview_theme_background_color(ui);
            bool bg_override              = ui.image_window_bg_override;
            if (ImGui::Checkbox("Override image background", &bg_override)
                && bg_override) {
                ui.image_window_bg_color = theme_background_color;
            }
            ui.image_window_bg_override = bg_override;

            ImVec4 background_color = ui.image_window_bg_override
                                          ? ui.image_window_bg_color
                                          : theme_background_color;
            ImGui::BeginDisabled(!ui.image_window_bg_override);
            if (ImGui::ColorEdit4("Image background", &background_color.x,
                                  color_flags)
                && ui.image_window_bg_override) {
                ui.image_window_bg_color = background_color;
            }
            ImGui::EndDisabled();
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

namespace {

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

    bool draw_preferences_segment_button(const char* id, const char* label,
                                         bool selected, bool enabled,
                                         float width)
    {
        ImGui::PushID(id);
        if (!enabled)
            ImGui::BeginDisabled();
        push_active_button_style(selected);
        const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
        pop_active_button_style(selected);
        if (!enabled)
            ImGui::EndDisabled();
        ImGui::PopID();
        return pressed && enabled;
    }

    void draw_theme_preferences_section(PlaceholderUiState& ui)
    {
        draw_section_heading(
            "Theme", UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        const AppStylePreset current_style_preset = sanitize_app_style_preset(
            ui.style_preset);
        if (ImGui::BeginCombo("##pref_ui_style",
                              app_style_preset_name(current_style_preset))) {
            for (int preset_value = static_cast<int>(AppStylePreset::IvLight);
                 preset_value <= static_cast<int>(AppStylePreset::ImGuiClassic);
                 ++preset_value) {
                const AppStylePreset preset = static_cast<AppStylePreset>(
                    preset_value);
                const bool selected = preset == current_style_preset;
                if (ImGui::Selectable(app_style_preset_name(preset), selected))
                    ui.style_preset = preset_value;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void draw_pixel_view_preferences_section(PlaceholderUiState& ui)
    {
        draw_section_heading(
            "Pixel View",
            UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        if (begin_two_column_table("##pref_pixel_view",
                                   UiMetrics::Preferences::kLabelColumnWidth,
                                   ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_NoSavedSettings)) {
            table_labeled_row("Follows mouse");
            draw_right_aligned_checkbox("##pref_pixelview_follows_mouse",
                                        ui.pixelview_follows_mouse);
            register_layout_dump_synthetic_item("text",
                                                "Pixel view follows mouse");

            table_labeled_row("Closeup pixels");
            draw_right_aligned_int_stepper(
                "pref_closeup_pixels", ui.closeup_pixels, 2, nullptr,
                UiMetrics::Preferences::kStepperButtonWidth,
                UiMetrics::Preferences::kStepperValueWidth);

            table_labeled_row("Closeup average");
            draw_right_aligned_int_stepper(
                "pref_closeup_avg_pixels", ui.closeup_avg_pixels, 2, nullptr,
                UiMetrics::Preferences::kStepperButtonWidth,
                UiMetrics::Preferences::kStepperValueWidth);
            ImGui::EndTable();
        }
    }

    void draw_image_rendering_preferences_section(PlaceholderUiState& ui)
    {
        draw_section_heading(
            "Image Rendering",
            UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        if (begin_two_column_table("##pref_image_rendering",
                                   UiMetrics::Preferences::kLabelColumnWidth,
                                   ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_NoSavedSettings)) {
            table_labeled_row("Linear interpolation");
            draw_right_aligned_checkbox("##pref_linear_interpolation",
                                        ui.linear_interpolation);
            ImGui::EndTable();
        }
    }

    void draw_ocio_config_source_selector(PlaceholderUiState& ui, float spacing)
    {
        const float row_width    = ImGui::GetContentRegionAvail().x;
        const float button_width = std::max(1.0f, (row_width - spacing * 2.0f)
                                                      / 3.0f);
        const int ocio_source    = ui.ocio_config_source;
        if (draw_preferences_segment_button(
                "ocio_cfg_global", "Global",
                ocio_source == static_cast<int>(OcioConfigSource::Global), true,
                button_width)) {
            ui.ocio_config_source = static_cast<int>(OcioConfigSource::Global);
        }
        ImGui::SameLine(0.0f, spacing);
        if (draw_preferences_segment_button(
                "ocio_cfg_builtin", "Built-in",
                ocio_source == static_cast<int>(OcioConfigSource::BuiltIn),
                true, button_width)) {
            ui.ocio_config_source = static_cast<int>(OcioConfigSource::BuiltIn);
        }
        ImGui::SameLine(0.0f, spacing);
        if (draw_preferences_segment_button(
                "ocio_cfg_user", "User",
                ocio_source == static_cast<int>(OcioConfigSource::User), true,
                button_width)) {
            ui.ocio_config_source = static_cast<int>(OcioConfigSource::User);
        }
    }

    void draw_ocio_user_config_selector(PlaceholderUiState& ui, float spacing)
    {
        if (static_cast<OcioConfigSource>(ui.ocio_config_source)
            != OcioConfigSource::User) {
            return;
        }

        if (!begin_two_column_table("##pref_ocio_user",
                                    UiMetrics::Preferences::kLabelColumnWidth,
                                    ImGuiTableFlags_SizingStretchProp
                                        | ImGuiTableFlags_NoSavedSettings)) {
            return;
        }

        table_labeled_row("Path");
        const float browse_width
            = UiMetrics::Preferences::kOcioBrowseButtonWidth;
        const float field_width = std::max(60.0f,
                                           ImGui::GetContentRegionAvail().x
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

    void draw_ocio_config_info(const OcioConfigSelection& ocio_selection)
    {
        if (!begin_two_column_table("##pref_ocio_info",
                                    UiMetrics::Preferences::kLabelColumnWidth,
                                    ImGuiTableFlags_SizingStretchProp
                                        | ImGuiTableFlags_NoSavedSettings)) {
            return;
        }

        table_labeled_row("Resolved source");
        draw_right_aligned_text(
            ocio_config_source_name(ocio_selection.resolved_source));

        if (!ocio_selection.resolved_path.empty()) {
            table_labeled_row("Resolved path");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(ocio_selection.resolved_path.c_str());
            ImGui::PopTextWrapPos();
        }
        if (ocio_selection.resolved_source == OcioConfigSource::Global
            && !ocio_selection.env_value.empty()) {
            table_labeled_row("OCIO env");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(ocio_selection.env_value.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTable();
    }

    void draw_ocio_config_status(const OcioConfigSelection& ocio_selection)
    {
        if (ocio_selection.requested_source == OcioConfigSource::Global
            && ocio_selection.resolved_source == OcioConfigSource::BuiltIn) {
            draw_disabled_wrapped_text(
                "$OCIO is missing or invalid. Built-in config will be used.");
            return;
        }
        if (ocio_selection.requested_source == OcioConfigSource::User
            && ocio_selection.fallback_applied) {
            draw_disabled_wrapped_text(
                "User config is missing or invalid. A fallback config will be used.");
        }
    }

    void draw_ocio_preferences_section(PlaceholderUiState& ui, float spacing)
    {
        draw_section_heading(
            "OCIO Config",
            UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        draw_ocio_config_source_selector(ui, spacing);
        draw_ocio_user_config_selector(ui, spacing);

        OcioConfigSelection ocio_selection;
        resolve_ocio_config_selection(ui, ocio_selection);
        draw_ocio_config_info(ocio_selection);
        draw_ocio_config_status(ocio_selection);
    }

    void draw_backend_selector_row(PlaceholderUiState& ui, float spacing,
                                   ImVec2& backend_row_min,
                                   ImVec2& backend_row_max,
                                   bool& have_backend_row_rect)
    {
        BackendKind requested_backend = sanitize_backend_kind(
            ui.renderer_backend);
        const float row_width    = std::max(1.0f,
                                            ImGui::GetContentRegionAvail().x);
        const float button_width = std::max(1.0f, (row_width - spacing * 3.0f)
                                                      / 4.0f);

        const bool auto_selected = requested_backend == BackendKind::Auto;
        if (draw_preferences_segment_button("pref_backend_auto", "Auto",
                                            auto_selected, true,
                                            button_width)) {
            ui.renderer_backend = static_cast<int>(BackendKind::Auto);
        }
        register_test_engine_item_label("pref-backend:auto");
        register_layout_dump_synthetic_item("button", "Renderer backend Auto");

        backend_row_min       = ImGui::GetItemRectMin();
        backend_row_max       = ImGui::GetItemRectMax();
        have_backend_row_rect = true;

        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            ImGui::SameLine(0.0f, spacing);
            requested_backend   = sanitize_backend_kind(ui.renderer_backend);
            const bool selected = requested_backend == info.build_info.kind;
            const bool enabled  = info.build_info.compiled && info.available;
            if (draw_preferences_segment_button(
                    backend_cli_name(info.build_info.kind),
                    backend_display_name(info.build_info.kind), selected,
                    enabled, button_width)) {
                ui.renderer_backend = static_cast<int>(info.build_info.kind);
            }
            const std::string test_label = std::string("pref-backend:")
                                           + backend_cli_name(
                                               info.build_info.kind);
            register_test_engine_item_label(test_label.c_str());
            register_layout_dump_synthetic_item(
                "button",
                Strutil::fmt::format("Renderer backend {}",
                                     backend_display_name(info.build_info.kind))
                    .c_str());
            const ImVec2 item_min = ImGui::GetItemRectMin();
            const ImVec2 item_max = ImGui::GetItemRectMax();
            backend_row_min.x     = std::min(backend_row_min.x, item_min.x);
            backend_row_min.y     = std::min(backend_row_min.y, item_min.y);
            backend_row_max.x     = std::max(backend_row_max.x, item_max.x);
            backend_row_max.y     = std::max(backend_row_max.y, item_max.y);
        }
    }

    void draw_backend_preference_info(PlaceholderUiState& ui,
                                      BackendKind active_backend)
    {
        const BackendKind requested_backend = sanitize_backend_kind(
            ui.renderer_backend);
        const BackendKind next_launch_backend = resolve_backend_request(
            requested_backend);
        if (!begin_two_column_table("##pref_system_info",
                                    UiMetrics::Preferences::kLabelColumnWidth,
                                    ImGuiTableFlags_SizingStretchProp
                                        | ImGuiTableFlags_NoSavedSettings)) {
            return;
        }

        const char* stored_preference = (requested_backend == BackendKind::Auto)
                                            ? "Auto"
                                            : backend_display_name(
                                                  requested_backend);
        table_labeled_row("Stored preference");
        draw_right_aligned_text(stored_preference);

        table_labeled_row("Current backend");
        draw_right_aligned_text(backend_display_name(active_backend));

        table_labeled_row("Next launch backend");
        draw_right_aligned_text(backend_display_name(next_launch_backend));

        table_labeled_row("Generate mipmaps");
        draw_right_aligned_checkbox("##pref_auto_mipmap", ui.auto_mipmap);
        ImGui::EndTable();
    }

    void draw_backend_status_messages(const PlaceholderUiState& ui,
                                      BackendKind active_backend)
    {
        const BackendKind requested_backend = sanitize_backend_kind(
            ui.renderer_backend);
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

        if (invalid_requested_backend) {
            draw_disabled_wrapped_text(
                "Requested backend is not built in this binary and will be ignored when Preferences closes.");
        } else if (unavailable_requested_backend) {
            const std::string_view unavailable_reason
                = backend_unavailable_reason(requested_backend);
            if (unavailable_reason.empty()) {
                draw_disabled_wrapped_text(
                    "Requested backend is unavailable at runtime and will be ignored when Preferences closes.");
            } else {
                draw_disabled_wrapped_text(
                    Strutil::fmt::format(
                        "Requested backend is unavailable at runtime ({}) and will be ignored when Preferences closes.",
                        unavailable_reason)
                        .c_str());
            }
        } else if (next_launch_backend != active_backend) {
            draw_disabled_wrapped_text("Backend change requires restart.");
        }
        if (requested_backend == BackendKind::Auto)
            draw_disabled_wrapped_text(
                "Auto selects the first available backend.");
        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            if (!info.build_info.compiled || info.available)
                continue;
            if (info.unavailable_reason.empty()) {
                draw_disabled_wrapped_text(
                    Strutil::fmt::format("{} unavailable",
                                         info.build_info.display_name)
                        .c_str());
            } else {
                draw_disabled_wrapped_text(
                    Strutil::fmt::format("{} unavailable: {}",
                                         info.build_info.display_name,
                                         info.unavailable_reason)
                        .c_str());
            }
        }
    }

    void draw_backend_preferences_section(PlaceholderUiState& ui,
                                          BackendKind active_backend,
                                          float spacing)
    {
        draw_section_heading(
            "System (required restart)",
            UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        ImVec2 backend_row_min(0.0f, 0.0f);
        ImVec2 backend_row_max(0.0f, 0.0f);
        bool have_backend_row_rect = false;
        draw_backend_selector_row(ui, spacing, backend_row_min, backend_row_max,
                                  have_backend_row_rect);
        if (have_backend_row_rect) {
            register_layout_dump_synthetic_rect("button", "Renderer backend",
                                                backend_row_min,
                                                backend_row_max);
        }
        draw_backend_preference_info(ui, active_backend);
        draw_backend_status_messages(ui, active_backend);
    }

    void draw_memory_preferences_section(PlaceholderUiState& ui)
    {
        draw_section_heading(
            "Memory", UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        if (begin_two_column_table("##pref_memory",
                                   UiMetrics::Preferences::kLabelColumnWidth,
                                   ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_NoSavedSettings)) {
            table_labeled_row("Image cache max memory");
            draw_right_aligned_int_stepper(
                "pref_max_mem", ui.max_memory_ic_mb, 64, "MB",
                UiMetrics::Preferences::kStepperButtonWidth,
                UiMetrics::Preferences::kStepperValueWidth);
            ImGui::EndTable();
        }
    }

    void draw_slideshow_preferences_section(PlaceholderUiState& ui)
    {
        draw_section_heading(
            "Slide Show",
            UiMetrics::Preferences::kSectionSeparatorTextPaddingY);
        if (begin_two_column_table("##pref_slideshow",
                                   UiMetrics::Preferences::kLabelColumnWidth,
                                   ImGuiTableFlags_SizingStretchProp
                                       | ImGuiTableFlags_NoSavedSettings)) {
            table_labeled_row("Delay");
            draw_right_aligned_int_stepper(
                "pref_slide_delay", ui.slide_duration_seconds, 1, "s",
                UiMetrics::Preferences::kStepperButtonWidth,
                UiMetrics::Preferences::kStepperValueWidth);
            ImGui::EndTable();
        }
    }

    void draw_preferences_close_button(bool& show_window)
    {
        ImGui::SetCursorPosY(
            ImGui::GetCursorPosY()
            + UiMetrics::AuxiliaryWindows::kPreferencesCloseGap);
        const float close_button_width
            = UiMetrics::Preferences::kCloseButtonWidth;
        const float x               = ImGui::GetCursorPosX();
        const float available_width = ImGui::GetContentRegionAvail().x;
        if (available_width > close_button_width) {
            ImGui::SetCursorPosX(
                x + (available_width - close_button_width) * 0.5f);
        }
        if (ImGui::Button("Close", ImVec2(close_button_width, 0.0f)))
            show_window = false;
    }

}  // namespace

void
draw_preferences_window(PlaceholderUiState& ui, bool& show_window,
                        BackendKind active_backend, bool reset_layout)
{
    if (!show_window)
        return;

    set_aux_window_defaults(UiMetrics::AuxiliaryWindows::kPreferencesOffset,
                            UiMetrics::AuxiliaryWindows::kPreferencesSize,
                            reset_layout);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        UiMetrics::kAuxWindowPadding);
    if (ImGui::Begin(k_preferences_window_title, &show_window)) {
        const float close_height = ImGui::GetFrameHeightWithSpacing();
        const float body_height
            = std::max(UiMetrics::AuxiliaryWindows::kPreferencesBodyMinHeight,
                       ImGui::GetContentRegionAvail().y - close_height
                           - UiMetrics::AuxiliaryWindows::kBodyBottomGap);
        ImGui::BeginChild("##iv_prefs_body", ImVec2(0.0f, body_height), false,
                          ImGuiWindowFlags_None);

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        draw_theme_preferences_section(ui);
        draw_pixel_view_preferences_section(ui);
        draw_image_rendering_preferences_section(ui);
        draw_ocio_preferences_section(ui, spacing);
        draw_backend_preferences_section(ui, active_backend, spacing);
        draw_memory_preferences_section(ui);
        draw_slideshow_preferences_section(ui);

        ImGui::EndChild();
        clamp_placeholder_ui_state(ui);
        draw_preferences_close_button(show_window);
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

}  // namespace Imiv
