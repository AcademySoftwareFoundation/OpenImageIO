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
