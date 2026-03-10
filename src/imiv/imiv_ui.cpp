// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_test_engine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <imgui.h>

#include <OpenImageIO/half.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

template<class T>
T
read_unaligned_value(const unsigned char* ptr)
{
    T value = {};
    std::memcpy(&value, ptr, sizeof(T));
    return value;
}

std::string
channel_label_for_index(int c)
{
    static const char* names[] = { "R", "G", "B", "A" };
    if (c >= 0 && c < 4)
        return names[c];
    return Strutil::fmt::format("C{}", c);
}

std::string
pixel_preview_channel_label(const LoadedImage& image, int c)
{
    if (c >= 0 && c < static_cast<int>(image.channel_names.size())
        && !image.channel_names[c].empty()) {
        return image.channel_names[c];
    }
    return channel_label_for_index(c);
}

double
probe_value_to_oiio_float(UploadDataType type, double value)
{
    switch (type) {
    case UploadDataType::UInt8: return value / 255.0;
    case UploadDataType::UInt16: return value / 65535.0;
    case UploadDataType::UInt32: return value / 4294967295.0;
    case UploadDataType::Half:
    case UploadDataType::Float:
    case UploadDataType::Double: return value;
    default: break;
    }
    return value;
}

enum class ProbeStatsSemantics : uint8_t { RawStored = 0, OIIOFloat = 1 };

bool
sample_loaded_pixel(const LoadedImage& image, int x, int y,
                    std::vector<double>& out_channels)
{
    out_channels.clear();
    if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0
        || image.channel_bytes == 0) {
        return false;
    }
    if (x < 0 || y < 0 || x >= image.width || y >= image.height)
        return false;

    const size_t channels  = static_cast<size_t>(image.nchannels);
    const size_t px_stride = channels * image.channel_bytes;
    const size_t row_start = static_cast<size_t>(y) * image.row_pitch_bytes;
    const size_t px_start  = static_cast<size_t>(x) * px_stride;
    const size_t offset    = row_start + px_start;
    if (offset + px_stride > image.pixels.size())
        return false;

    out_channels.resize(channels);
    const unsigned char* src = image.pixels.data() + offset;
    for (size_t c = 0; c < channels; ++c) {
        const unsigned char* channel_ptr = src + c * image.channel_bytes;
        double v                         = 0.0;
        switch (image.type) {
        case UploadDataType::UInt8:
            v = static_cast<double>(*channel_ptr);
            break;
        case UploadDataType::UInt16:
            v = static_cast<double>(
                read_unaligned_value<uint16_t>(channel_ptr));
            break;
        case UploadDataType::UInt32:
            v = static_cast<double>(
                read_unaligned_value<uint32_t>(channel_ptr));
            break;
        case UploadDataType::Half:
            v = static_cast<double>(
                static_cast<float>(read_unaligned_value<half>(channel_ptr)));
            break;
        case UploadDataType::Float:
            v = static_cast<double>(read_unaligned_value<float>(channel_ptr));
            break;
        case UploadDataType::Double:
            v = read_unaligned_value<double>(channel_ptr);
            break;
        default: return false;
        }
        out_channels[c] = v;
    }
    return true;
}

bool
sample_loaded_pixel_with_semantics(const LoadedImage& image, int x, int y,
                                   ProbeStatsSemantics semantics,
                                   std::vector<double>& out_channels)
{
    if (!sample_loaded_pixel(image, x, y, out_channels))
        return false;
    if (semantics == ProbeStatsSemantics::OIIOFloat) {
        for (double& value : out_channels)
            value = probe_value_to_oiio_float(image.type, value);
    }
    return true;
}

bool
compute_area_stats(const LoadedImage& image, int center_x, int center_y,
                   int window_size, std::vector<double>& out_min,
                   std::vector<double>& out_max, std::vector<double>& out_avg,
                   int& out_samples,
                   ProbeStatsSemantics semantics
                   = ProbeStatsSemantics::RawStored)
{
    out_min.clear();
    out_max.clear();
    out_avg.clear();
    out_samples = 0;
    if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0)
        return false;
    if (window_size <= 0)
        return false;
    if ((window_size & 1) == 0)
        ++window_size;

    const int half_window = window_size / 2;
    const int x0          = std::max(0, center_x - half_window);
    const int x1          = std::min(image.width - 1, center_x + half_window);
    const int y0          = std::max(0, center_y - half_window);
    const int y1          = std::min(image.height - 1, center_y + half_window);
    if (x1 < x0 || y1 < y0)
        return false;

    const size_t channels = static_cast<size_t>(image.nchannels);
    out_min.assign(channels, std::numeric_limits<double>::infinity());
    out_max.assign(channels, -std::numeric_limits<double>::infinity());
    out_avg.assign(channels, 0.0);

    std::vector<double> sample;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!sample_loaded_pixel_with_semantics(image, x, y, semantics,
                                                    sample))
                continue;
            if (sample.size() != channels)
                continue;
            for (size_t c = 0; c < channels; ++c) {
                out_min[c] = std::min(out_min[c], sample[c]);
                out_max[c] = std::max(out_max[c], sample[c]);
                out_avg[c] += sample[c];
            }
            ++out_samples;
        }
    }

    if (out_samples <= 0) {
        out_min.clear();
        out_max.clear();
        out_avg.clear();
        return false;
    }
    for (double& value : out_avg)
        value /= static_cast<double>(out_samples);
    return true;
}

const char*
channel_view_name(int mode)
{
    switch (mode) {
    case 0: return "Full Color";
    case 1: return "Red";
    case 2: return "Green";
    case 3: return "Blue";
    case 4: return "Alpha";
    default: break;
    }
    return "Unknown";
}

const char*
color_mode_name(int mode)
{
    switch (mode) {
    case 0: return "RGBA";
    case 1: return "RGB";
    case 2: return "Single channel";
    case 3: return "Luminance";
    case 4: return "Heatmap";
    default: break;
    }
    return "Unknown";
}

const char*
mouse_mode_name(int mode)
{
    switch (mode) {
    case 0: return "Zoom";
    case 1: return "Pan";
    case 2: return "Wipe";
    case 3: return "Select";
    case 4: return "Annotate";
    default: break;
    }
    return "Zoom";
}

void
draw_padded_message(const char* message, float x_pad, float y_pad)
{
    if (!message || message[0] == '\0')
        return;
    ImVec2 pos = ImGui::GetCursorPos();
    pos.x += x_pad;
    pos.y += y_pad;
    ImGui::SetCursorPos(pos);
    const float wrap_width = ImGui::GetCursorPosX()
                             + std::max(64.0f, ImGui::GetContentRegionAvail().x
                                                   - x_pad);
    ImGui::PushTextWrapPos(wrap_width);
    ImGui::TextUnformatted(message);
    ImGui::PopTextWrapPos();
}

std::string
format_probe_channel_value(UploadDataType type, double value)
{
    switch (type) {
    case UploadDataType::UInt8:
    case UploadDataType::UInt16:
    case UploadDataType::UInt32: return Strutil::fmt::format("{:.0f}", value);
    case UploadDataType::Half:
    case UploadDataType::Float: return Strutil::fmt::format("{:.7g}", value);
    case UploadDataType::Double: return Strutil::fmt::format("{:.12g}", value);
    default: break;
    }
    return Strutil::fmt::format("{:.7g}", value);
}

std::string
format_probe_fixed3(double value)
{
    return Strutil::fmt::format("{:.3f}", value);
}

std::string
format_probe_iv_float(double value)
{
    if (value < 10.0)
        return Strutil::fmt::format("{:.3f}", value);
    if (value < 100.0)
        return Strutil::fmt::format("{:.2f}", value);
    if (value < 1000.0)
        return Strutil::fmt::format("{:.1f}", value);
    return Strutil::fmt::format("{:.0f}", value);
}

std::string
format_probe_integer_trunc(UploadDataType type, double value)
{
    switch (type) {
    case UploadDataType::UInt8:
        return Strutil::fmt::format("{}", static_cast<unsigned int>(
                                              std::clamp(value, 0.0, 255.0)));
    case UploadDataType::UInt16:
        return Strutil::fmt::format("{}", static_cast<unsigned int>(
                                              std::clamp(value, 0.0, 65535.0)));
    case UploadDataType::UInt32:
        return Strutil::fmt::format("{}",
                                    static_cast<uint32_t>(
                                        std::clamp(value, 0.0, 4294967295.0)));
    default: break;
    }
    return Strutil::fmt::format("{:.0f}", value);
}

bool
probe_type_is_integer(UploadDataType type)
{
    return type == UploadDataType::UInt8 || type == UploadDataType::UInt16;
}

double
probe_channel_integer_denominator(UploadDataType type)
{
    switch (type) {
    case UploadDataType::UInt8: return 255.0;
    case UploadDataType::UInt16: return 65535.0;
    case UploadDataType::UInt32: return 4294967295.0;
    default: break;
    }
    return 1.0;
}
OverlayPanelRect
draw_overlay_text_panel(const std::vector<std::string>& lines,
                        const ImVec2& preferred_pos, const ImVec2& clip_min,
                        const ImVec2& clip_max, ImFont* font = nullptr)
{
    OverlayPanelRect panel;
    if (lines.empty())
        return panel;

    ImFont* draw_font     = font ? font : ImGui::GetFont();
    const float font_size = draw_font ? draw_font->LegacySize
                                      : ImGui::GetFontSize();
    const float pad_x     = 10.0f;
    const float pad_y     = 8.0f;
    const float line_gap  = 2.0f;
    const float line_h    = draw_font ? draw_font->LegacySize
                                      : ImGui::GetTextLineHeight();

    float text_w = 0.0f;
    for (const std::string& line : lines) {
        const ImVec2 size = draw_font->CalcTextSizeA(
            font_size, std::numeric_limits<float>::max(), 0.0f, line.c_str());
        if (size.x > text_w)
            text_w = size.x;
    }
    const float panel_w = text_w + pad_x * 2.0f;
    const float panel_h = pad_y * 2.0f
                          + static_cast<float>(lines.size()) * line_h
                          + static_cast<float>(lines.size() - 1) * line_gap;

    const float min_x = std::min(clip_min.x, clip_max.x);
    const float min_y = std::min(clip_min.y, clip_max.y);
    const float max_x = std::max(clip_min.x, clip_max.x);
    const float max_y = std::max(clip_min.y, clip_max.y);
    if ((max_x - min_x) < panel_w || (max_y - min_y) < panel_h)
        return panel;

    ImVec2 pos = preferred_pos;
    pos.x      = std::clamp(pos.x, min_x, std::max(min_x, max_x - panel_w));
    pos.y      = std::clamp(pos.y, min_y, std::max(min_y, max_y - panel_h));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(clip_min, clip_max, true);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + panel_w, pos.y + panel_h),
                             IM_COL32(20, 24, 30, 224), 4.0f);
    draw_list->AddRect(pos, ImVec2(pos.x + panel_w, pos.y + panel_h),
                       IM_COL32(175, 185, 205, 255), 4.0f, 0, 1.0f);

    ImVec2 text_pos(pos.x + pad_x, pos.y + pad_y);
    for (const std::string& line : lines) {
        draw_list->AddText(draw_font, font_size, text_pos,
                           IM_COL32(240, 242, 245, 255), line.c_str());
        text_pos.y += line_h + line_gap;
    }
    draw_list->PopClipRect();

    panel.valid = true;
    panel.min   = pos;
    panel.max   = ImVec2(pos.x + panel_w, pos.y + panel_h);
    return panel;
}

void
set_aux_window_defaults(const ImVec2& offset, const ImVec2& size)
{
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImVec2 base_pos(0.0f, 0.0f);
    if (main_viewport != nullptr)
        base_pos = main_viewport->WorkPos;
    ImGui::SetNextWindowPos(ImVec2(base_pos.x + offset.x, base_pos.y + offset.y),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
}



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

                auto draw_row = [](const char* label,
                                   const std::string& value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(value.c_str());
                    ImGui::PopTextWrapPos();
                };

                draw_row("Path", viewer.image.path);
                for (const auto& row : viewer.image.longinfo_rows) {
                    draw_row(row.first.c_str(), row.second);
                }
                draw_row("Orientation",
                         Strutil::fmt::format("{}", viewer.image.orientation));
                draw_row("Subimage",
                         Strutil::fmt::format("{}/{}",
                                              viewer.image.subimage + 1,
                                              viewer.image.nsubimages));
                draw_row("MIP level",
                         Strutil::fmt::format("{}/{}",
                                              viewer.image.miplevel + 1,
                                              viewer.image.nmiplevels));
                draw_row("Row pitch (bytes)",
                         Strutil::fmt::format("{}",
                                              viewer.image.row_pitch_bytes));
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
push_preview_active_button_style(bool active)
{
    if (!active)
        return;
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 112, 171, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 133, 200, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(57, 95, 146, 255));
}

void
pop_preview_active_button_style(bool active)
{
    if (!active)
        return;
    ImGui::PopStyleColor(3);
}

void
preview_form_next_row(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
}

bool
draw_preview_row_button_cell(const char* label, bool active)
{
    ImGui::TableNextColumn();
    push_preview_active_button_style(active);
    const bool pressed
        = ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
    pop_preview_active_button_style(active);
    return pressed;
}

void
preview_set_rgb_mode(PlaceholderUiState& ui)
{
    ui.color_mode      = 1;
    ui.current_channel = 0;
}

void
preview_set_luma_mode(PlaceholderUiState& ui)
{
    ui.color_mode      = 3;
    ui.current_channel = 0;
}

void
preview_set_single_channel_mode(PlaceholderUiState& ui, int channel)
{
    ui.color_mode      = 2;
    ui.current_channel = channel;
}

void
preview_set_heat_mode(PlaceholderUiState& ui)
{
    ui.color_mode = 4;
    if (ui.current_channel <= 0)
        ui.current_channel = 1;
}

void
preview_reset_adjustments(PlaceholderUiState& ui)
{
    ui.exposure = 0.0f;
    ui.gamma    = 1.0f;
    ui.offset   = 0.0f;
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
                          ImGuiWindowFlags_NoScrollbar);

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
                if (draw_preview_row_button_cell("RGB", rgb_active)) {
                    preview_set_rgb_mode(ui);
                }
                if (draw_preview_row_button_cell("Luma",
                                                 ui.color_mode == 3
                                                     && ui.current_channel
                                                            == 0)) {
                    preview_set_luma_mode(ui);
                }
                if (draw_preview_row_button_cell("R", red_active)) {
                    preview_set_single_channel_mode(ui, 1);
                }
                if (draw_preview_row_button_cell("G", green_active)) {
                    preview_set_single_channel_mode(ui, 2);
                }
                if (draw_preview_row_button_cell("B", blue_active)) {
                    preview_set_single_channel_mode(ui, 3);
                }
                if (draw_preview_row_button_cell("A", alpha_active)) {
                    preview_set_single_channel_mode(ui, 4);
                }
                if (draw_preview_row_button_cell("Heat", ui.color_mode == 4)) {
                    preview_set_heat_mode(ui);
                }
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



OverlayPanelRect
draw_pixel_closeup_overlay(const ViewerState& viewer,
                           PlaceholderUiState& ui_state,
                           const ImageCoordinateMap& map,
                           ImTextureRef closeup_texture,
                           bool has_closeup_texture, const AppFonts& fonts)
{
    OverlayPanelRect panel;
    if (!ui_state.show_pixelview_window || !map.valid)
        return panel;

    std::vector<std::string> lines;
    std::vector<ImU32> line_colors;
    lines.emplace_back("Pixel Closeup:");
    line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
    if (viewer.image.path.empty()) {
        lines.emplace_back("No image loaded.");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
    } else if (!viewer.probe_valid) {
        lines.emplace_back("Hover over image to inspect.");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
    } else {
        lines.emplace_back(Strutil::fmt::format("          ({:d},{:d})",
                                                viewer.probe_x,
                                                viewer.probe_y));
        line_colors.emplace_back(IM_COL32(0, 255, 255, 220));

        std::vector<double> min_values;
        std::vector<double> max_values;
        std::vector<double> avg_values;
        int sample_count        = 0;
        const bool integer_type = probe_type_is_integer(viewer.image.type);
        const ProbeStatsSemantics preview_semantics
            = integer_type ? ProbeStatsSemantics::RawStored
                           : ProbeStatsSemantics::OIIOFloat;
        const bool have_stats
            = compute_area_stats(viewer.image, viewer.probe_x, viewer.probe_y,
                                 ui_state.closeup_avg_pixels, min_values,
                                 max_values, avg_values, sample_count,
                                 preview_semantics);

        const double denom = probe_channel_integer_denominator(
            viewer.image.type);
        if (integer_type) {
            lines.emplace_back("      Val    Norm   Min   Max   Avg");
        } else {
            lines.emplace_back("      Val    Min    Max    Avg");
        }
        line_colors.emplace_back(IM_COL32(255, 255, 160, 220));
        for (size_t c = 0; c < viewer.probe_channels.size(); ++c) {
            const std::string label
                = pixel_preview_channel_label(viewer.image,
                                              static_cast<int>(c));
            const double semantic_value
                = integer_type
                      ? viewer.probe_channels[c]
                      : probe_value_to_oiio_float(viewer.image.type,
                                                  viewer.probe_channels[c]);
            const std::string value
                = integer_type
                      ? format_probe_integer_trunc(viewer.image.type,
                                                   viewer.probe_channels[c])
                      : format_probe_iv_float(semantic_value);
            if (have_stats && c < min_values.size() && c < max_values.size()
                && c < avg_values.size()) {
                if (integer_type && denom > 0.0) {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<2}: {:>5}  {:>6.3f}  {:>3}  {:>3}  {:>3}", label,
                        value, viewer.probe_channels[c] / denom,
                        format_probe_integer_trunc(viewer.image.type,
                                                   min_values[c]),
                        format_probe_integer_trunc(viewer.image.type,
                                                   max_values[c]),
                        format_probe_integer_trunc(viewer.image.type,
                                                   avg_values[c])));
                } else {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<2}: {:>6}  {:>6}  {:>6}  {:>6}", label, value,
                        format_probe_iv_float(min_values[c]),
                        format_probe_iv_float(max_values[c]),
                        format_probe_iv_float(avg_values[c])));
                }
            } else {
                if (integer_type) {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<2}: {:>5}  {:>6}  {:>3}  {:>3}  {:>3}", label,
                        value, "-----", "---", "---", "---"));
                } else {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<2}: {:>6}  {:>6}  {:>6}  {:>6}", label, value,
                        "-----", "-----", "-----"));
                }
            }
            ImU32 channel_color = IM_COL32(220, 220, 220, 255);
            if (!label.empty()) {
                if (label[0] == 'R')
                    channel_color = IM_COL32(250, 94, 143, 255);
                else if (label[0] == 'G')
                    channel_color = IM_COL32(135, 203, 124, 255);
                else if (label[0] == 'B')
                    channel_color = IM_COL32(107, 188, 255, 255);
            }
            line_colors.emplace_back(channel_color);
        }
        (void)sample_count;
    }

    const float closeup_window_size = 260.0f;
    const float follow_mouse_offset = 15.0f;
    const float corner_padding      = 5.0f;
    const float text_pad_x          = 10.0f;
    const float text_pad_y          = 8.0f;
    const float text_line_gap       = 2.0f;
    const float text_to_window_gap  = 2.0f;
    const float text_wrap_w         = std::max(8.0f,
                                               closeup_window_size - text_pad_x * 2.0f);
    ImFont* text_font          = fonts.mono ? fonts.mono : ImGui::GetFont();
    const float text_font_size = text_font ? text_font->LegacySize
                                           : ImGui::GetFontSize();

    float text_panel_h = text_pad_y * 2.0f;
    for (const std::string& line : lines) {
        const ImVec2 line_size
            = text_font->CalcTextSizeA(text_font_size,
                                       std::numeric_limits<float>::max(),
                                       text_wrap_w, line.c_str());
        text_panel_h += line_size.y;
        text_panel_h += text_line_gap;
    }
    if (!lines.empty())
        text_panel_h -= text_line_gap;
    const float text_panel_w = closeup_window_size;
    const float total_h      = closeup_window_size + text_to_window_gap
                          + text_panel_h;

    const float clip_min_x = std::min(map.viewport_rect_min.x,
                                      map.viewport_rect_max.x);
    const float clip_min_y = std::min(map.viewport_rect_min.y,
                                      map.viewport_rect_max.y);
    const float clip_max_x = std::max(map.viewport_rect_min.x,
                                      map.viewport_rect_max.x);
    const float clip_max_y = std::max(map.viewport_rect_min.y,
                                      map.viewport_rect_max.y);
    if ((clip_max_x - clip_min_x) < closeup_window_size
        || (clip_max_y - clip_min_y) < total_h) {
        return panel;
    }

    ImVec2 closeup_min(clip_min_x + corner_padding,
                       clip_min_y + corner_padding);
    const ImVec2 mouse_pos = ImGui::GetIO().MousePos;

    if (ui_state.pixelview_follows_mouse) {
        const bool should_show_on_left = (mouse_pos.x + closeup_window_size
                                          + follow_mouse_offset)
                                         > clip_max_x;
        const bool should_show_above = (mouse_pos.y + closeup_window_size
                                        + follow_mouse_offset + text_panel_h)
                                       > clip_max_y;

        closeup_min.x = mouse_pos.x + follow_mouse_offset;
        closeup_min.y = mouse_pos.y + follow_mouse_offset;
        if (should_show_on_left) {
            closeup_min.x = mouse_pos.x - follow_mouse_offset
                            - closeup_window_size;
        }
        if (should_show_above) {
            closeup_min.y = mouse_pos.y - follow_mouse_offset
                            - closeup_window_size - text_to_window_gap
                            - text_panel_h;
        }
    } else {
        closeup_min.x = ui_state.pixelview_left_corner
                            ? (clip_min_x + corner_padding)
                            : (clip_max_x - closeup_window_size
                               - corner_padding);
        closeup_min.y = clip_min_y + corner_padding;

        const ImVec2 panel_max(closeup_min.x + text_panel_w,
                               closeup_min.y + total_h);
        const bool mouse_over_panel = mouse_pos.x >= closeup_min.x
                                      && mouse_pos.x <= panel_max.x
                                      && mouse_pos.y >= closeup_min.y
                                      && mouse_pos.y <= panel_max.y;
        if (mouse_over_panel) {
            ui_state.pixelview_left_corner = !ui_state.pixelview_left_corner;
            closeup_min.x                  = ui_state.pixelview_left_corner
                                                 ? (clip_min_x + corner_padding)
                                                 : (clip_max_x - closeup_window_size
                                   - corner_padding);
        }
    }

    closeup_min.x = std::clamp(closeup_min.x, clip_min_x,
                               std::max(clip_min_x, clip_max_x - text_panel_w));
    closeup_min.y = std::clamp(closeup_min.y, clip_min_y,
                               std::max(clip_min_y, clip_max_y - total_h));
    const ImVec2 closeup_max(closeup_min.x + closeup_window_size,
                             closeup_min.y + closeup_window_size);
    const ImVec2 text_min(closeup_min.x, closeup_max.y + text_to_window_gap);
    const ImVec2 text_max(text_min.x + text_panel_w, text_min.y + text_panel_h);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(map.viewport_rect_min, map.viewport_rect_max, true);
    draw_list->AddRectFilled(closeup_min, closeup_max,
                             IM_COL32(20, 24, 30, 224), 4.0f);
    draw_list->AddRect(closeup_min, closeup_max, IM_COL32(175, 185, 205, 255),
                       4.0f, 0, 1.0f);

    const bool render_zoom_patch = has_closeup_texture
                                   && !viewer.image.path.empty()
                                   && viewer.probe_valid && map.source_width > 0
                                   && map.source_height > 0;
    if (render_zoom_patch) {
        int display_w = viewer.image.width;
        int display_h = viewer.image.height;
        oriented_image_dimensions(viewer.image, display_w, display_h);
        display_w = std::max(1, display_w);
        display_h = std::max(1, display_h);

        int closeup_px = std::clamp(ui_state.closeup_pixels, 1,
                                    std::min(display_w, display_h));
        if (closeup_px <= 0)
            closeup_px = 1;
        int patch_w = std::min(closeup_px, display_w);
        int patch_h = std::min(closeup_px, display_h);
        patch_w     = std::max(1, patch_w);
        patch_h     = std::max(1, patch_h);

        const ImVec2 source_uv((static_cast<float>(viewer.probe_x) + 0.5f)
                                   / static_cast<float>(map.source_width),
                               (static_cast<float>(viewer.probe_y) + 0.5f)
                                   / static_cast<float>(map.source_height));
        const ImVec2 display_uv = source_uv_to_display_uv(source_uv,
                                                          map.orientation);
        const int center_x
            = std::clamp(static_cast<int>(std::floor(display_uv.x * display_w)),
                         0, display_w - 1);
        const int center_y
            = std::clamp(static_cast<int>(std::floor(display_uv.y * display_h)),
                         0, display_h - 1);

        const int xbegin = std::clamp(center_x - patch_w / 2, 0,
                                      std::max(0, display_w - patch_w));
        const int ybegin = std::clamp(center_y - patch_h / 2, 0,
                                      std::max(0, display_h - patch_h));
        const int xend   = xbegin + patch_w;
        const int yend   = ybegin + patch_h;

        const ImVec2 uv_min(static_cast<float>(xbegin) / display_w,
                            static_cast<float>(ybegin) / display_h);
        const ImVec2 uv_max(static_cast<float>(xend) / display_w,
                            static_cast<float>(yend) / display_h);
        draw_list->AddImage(closeup_texture, closeup_min, closeup_max, uv_min,
                            uv_max, IM_COL32_WHITE);

        const float cell_w = closeup_window_size / patch_w;
        const float cell_h = closeup_window_size / patch_h;
        for (int i = 1; i < patch_w; ++i) {
            const float x = closeup_min.x + i * cell_w;
            draw_list->AddLine(ImVec2(x, closeup_min.y),
                               ImVec2(x, closeup_max.y),
                               IM_COL32(8, 10, 12, 140), 1.0f);
        }
        for (int i = 1; i < patch_h; ++i) {
            const float y = closeup_min.y + i * cell_h;
            draw_list->AddLine(ImVec2(closeup_min.x, y),
                               ImVec2(closeup_max.x, y),
                               IM_COL32(8, 10, 12, 140), 1.0f);
        }

        auto draw_corner_marker = [draw_list](const ImVec2& p0,
                                              const ImVec2& p1, ImU32 color) {
            const float corner_size = 4.0f;
            draw_list->AddLine(p0, ImVec2(p0.x + corner_size, p0.y), color,
                               1.0f);
            draw_list->AddLine(p0, ImVec2(p0.x, p0.y + corner_size), color,
                               1.0f);
            draw_list->AddLine(ImVec2(p1.x - corner_size, p0.y),
                               ImVec2(p1.x, p0.y), color, 1.0f);
            draw_list->AddLine(ImVec2(p1.x, p0.y),
                               ImVec2(p1.x, p0.y + corner_size), color, 1.0f);
            draw_list->AddLine(ImVec2(p0.x, p1.y - corner_size),
                               ImVec2(p0.x, p1.y), color, 1.0f);
            draw_list->AddLine(ImVec2(p0.x, p1.y),
                               ImVec2(p0.x + corner_size, p1.y), color, 1.0f);
            draw_list->AddLine(ImVec2(p1.x - corner_size, p1.y), p1, color,
                               1.0f);
            draw_list->AddLine(ImVec2(p1.x, p1.y - corner_size), p1, color,
                               1.0f);
        };

        const int center_ix = center_x - xbegin;
        const int center_iy = center_y - ybegin;
        const ImVec2 center_min(closeup_min.x + center_ix * cell_w,
                                closeup_min.y + center_iy * cell_h);
        const ImVec2 center_max(center_min.x + cell_w, center_min.y + cell_h);
        draw_corner_marker(center_min, center_max, IM_COL32(0, 255, 255, 180));

        int avg_px = std::clamp(ui_state.closeup_avg_pixels, 1,
                                std::min(patch_w, patch_h));
        if ((avg_px & 1) == 0)
            avg_px = std::max(1, avg_px - 1);
        if (avg_px > 1) {
            int avg_start_x = center_ix - avg_px / 2;
            int avg_start_y = center_iy - avg_px / 2;
            int avg_end_x   = avg_start_x + avg_px;
            int avg_end_y   = avg_start_y + avg_px;
            avg_start_x     = std::clamp(avg_start_x, 0, patch_w - avg_px);
            avg_start_y     = std::clamp(avg_start_y, 0, patch_h - avg_px);
            avg_end_x       = avg_start_x + avg_px;
            avg_end_y       = avg_start_y + avg_px;
            const ImVec2 avg_min(closeup_min.x + avg_start_x * cell_w,
                                 closeup_min.y + avg_start_y * cell_h);
            const ImVec2 avg_max(closeup_min.x + avg_end_x * cell_w,
                                 closeup_min.y + avg_end_y * cell_h);
            draw_corner_marker(avg_min, avg_max, IM_COL32(255, 255, 0, 170));
        }
    }

    draw_list->AddRectFilled(text_min, text_max, IM_COL32(20, 24, 30, 224),
                             4.0f);
    draw_list->AddRect(text_min, text_max, IM_COL32(175, 185, 205, 255), 4.0f,
                       0, 1.0f);
    ImVec2 text_pos(text_min.x + text_pad_x, text_min.y + text_pad_y);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const ImU32 color       = i < line_colors.size()
                                      ? line_colors[i]
                                      : IM_COL32(240, 242, 245, 255);
        draw_list->AddText(text_font, text_font_size, text_pos, color,
                           line.c_str(), nullptr, text_wrap_w);
        const ImVec2 line_size
            = text_font->CalcTextSizeA(text_font_size,
                                       std::numeric_limits<float>::max(),
                                       text_wrap_w, line.c_str());
        text_pos.y += line_size.y + text_line_gap;
    }
    draw_list->PopClipRect();

    panel.valid = true;
    panel.min   = closeup_min;
    panel.max   = ImVec2(closeup_min.x + text_panel_w, closeup_min.y + total_h);
    register_layout_dump_synthetic_item("text", "Pixel Closeup overlay");
    return panel;
}



void
draw_area_probe_overlay(const ViewerState& viewer,
                        const PlaceholderUiState& ui_state,
                        const ImageCoordinateMap& map,
                        const OverlayPanelRect& pixel_overlay_panel,
                        const AppFonts& fonts)
{
    (void)pixel_overlay_panel;
    if (!ui_state.show_area_probe_window || !map.valid)
        return;

    std::vector<std::string> lines;
    lines.emplace_back("Area Probe:");
    if (viewer.image.path.empty()) {
        lines.emplace_back("No image loaded.");
    } else {
        std::vector<double> min_values;
        std::vector<double> max_values;
        std::vector<double> avg_values;
        int sample_count = 0;
        const bool have_stats
            = viewer.probe_valid
              && compute_area_stats(viewer.image, viewer.probe_x,
                                    viewer.probe_y, ui_state.closeup_avg_pixels,
                                    min_values, max_values, avg_values,
                                    sample_count,
                                    ProbeStatsSemantics::OIIOFloat);

        const int channel_count = std::max(1, viewer.image.nchannels);
        for (int c = 0; c < channel_count; ++c) {
            const std::string channel
                = pixel_preview_channel_label(viewer.image,
                                              static_cast<int>(c));
            if (have_stats && static_cast<size_t>(c) < min_values.size()
                && static_cast<size_t>(c) < max_values.size()
                && static_cast<size_t>(c) < avg_values.size()) {
                lines.emplace_back(Strutil::fmt::format(
                    "{:<5}: [min: {:>6.3f}  max: {:>6.3f}  avg: {:>6.3f}]",
                    channel, min_values[c], max_values[c], avg_values[c]));
            } else {
                lines.emplace_back(Strutil::fmt::format(
                    "{:<5}: [min:  -----  max:  -----  avg:  -----]", channel));
            }
        }
    }

    const float clip_min_x  = std::min(map.viewport_rect_min.x,
                                       map.viewport_rect_max.x);
    const float clip_max_y  = std::max(map.viewport_rect_min.y,
                                       map.viewport_rect_max.y);
    const float pad_y       = 8.0f;
    const float line_gap    = 2.0f;
    const ImFont* mono_font = fonts.mono ? fonts.mono : ImGui::GetFont();
    const float line_h      = mono_font ? mono_font->LegacySize
                                        : ImGui::GetTextLineHeight();
    float panel_h = pad_y * 2.0f + static_cast<float>(lines.size()) * line_h
                    + static_cast<float>(std::max<size_t>(0, lines.size() - 1))
                          * line_gap;
    const float border_margin = 9.0f;
    ImVec2 preferred(clip_min_x + border_margin,
                     clip_max_y - panel_h - border_margin);
    draw_overlay_text_panel(lines, preferred, map.viewport_rect_min,
                            map.viewport_rect_max, fonts.mono);
    register_layout_dump_synthetic_item("text", "Area Probe overlay");
}

std::string
status_image_text(const ViewerState& viewer)
{
    if (viewer.image.path.empty())
        return "No image loaded";
    int current = 1;
    int total   = 1;
    if (!viewer.sibling_images.empty() && viewer.sibling_index >= 0) {
        total   = static_cast<int>(viewer.sibling_images.size());
        current = viewer.sibling_index + 1;
    }
    return Strutil::fmt::format("({}/{}): {} ({}x{}, {} ch, {})", current,
                                total, viewer.image.path, viewer.image.width,
                                viewer.image.height, viewer.image.nchannels,
                                upload_data_type_name(viewer.image.type));
}

std::string
status_view_text(const ViewerState& viewer, const PlaceholderUiState& ui)
{
    if (viewer.image.path.empty())
        return "";

    std::string mode = color_mode_name(ui.color_mode);
    if (ui.color_mode == 2 || ui.color_mode == 4) {
        mode += Strutil::fmt::format(" {}", ui.current_channel);
    } else {
        mode += Strutil::fmt::format(" ({})",
                                     channel_view_name(ui.current_channel));
    }

    const float zoom  = std::max(viewer.zoom, 0.00001f);
    const float z_num = zoom >= 1.0f ? zoom : 1.0f;
    const float z_den = zoom >= 1.0f ? 1.0f : (1.0f / zoom);
    std::string text  = Strutil::fmt::format(
        "{}  {:.2f}:{:.2f}  exp {:+.1f}  gam {:.2f}  off {:+.2f}", mode, z_num,
        z_den, ui.exposure, ui.gamma, ui.offset);
    if (viewer.image.nsubimages > 1) {
        text += Strutil::fmt::format("  subimg {}/{}",
                                     viewer.image.subimage + 1,
                                     viewer.image.nsubimages);
    }
    if (viewer.image.nmiplevels > 1) {
        text += Strutil::fmt::format("  MIP {}/{}", viewer.image.miplevel + 1,
                                     viewer.image.nmiplevels);
    }
    if (viewer.image.orientation != 1) {
        text += Strutil::fmt::format("  orient {}", viewer.image.orientation);
    }
    if (ui.show_mouse_mode_selector) {
        text += Strutil::fmt::format("  mouse {}",
                                     mouse_mode_name(ui.mouse_mode));
    }
    return text;
}

void
draw_embedded_status_bar(const ViewerState& viewer, PlaceholderUiState& ui)
{
    const std::string img_text  = status_image_text(viewer);
    const std::string view_text = status_view_text(viewer, ui);
    const bool show_progress    = false;

    int columns = 2;
    if (show_progress)
        ++columns;
    if (ui.show_mouse_mode_selector)
        ++columns;
    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV
                                  | ImGuiTableFlags_PadOuterX
                                  | ImGuiTableFlags_SizingStretchProp
                                  | ImGuiTableFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
    if (ImGui::BeginTable("##imiv_status_bar", columns, table_flags)) {
        ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch,
                                2.5f);
        ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthStretch,
                                2.0f);
        if (show_progress) {
            ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed,
                                    140.0f);
        }
        if (ui.show_mouse_mode_selector) {
            ImGui::TableSetupColumn("Mouse", ImGuiTableColumnFlags_WidthFixed,
                                    150.0f);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(img_text.c_str());
        register_layout_dump_synthetic_item("text", img_text.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(view_text.c_str());
        register_layout_dump_synthetic_item("text", view_text.c_str());

        if (show_progress) {
            ImGui::TableNextColumn();
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "idle");
        }

        if (ui.show_mouse_mode_selector) {
            static const char* mouse_modes[] = { "Zoom", "Pan", "Wipe",
                                                 "Select", "Annotate" };
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::Combo("##mouse_mode", &ui.mouse_mode, mouse_modes,
                         IM_ARRAYSIZE(mouse_modes));
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

}  // namespace Imiv
