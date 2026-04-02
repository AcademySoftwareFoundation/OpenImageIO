// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"

#include "imiv_actions.h"
#include "imiv_test_engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    enum class ProbeStatsSemantics : uint8_t { RawStored = 0, OIIOFloat = 1 };

    std::string channel_label_for_index(int c)
    {
        static const char* names[] = { "R", "G", "B", "A" };
        if (c >= 0 && c < 4)
            return names[c];
        return Strutil::fmt::format("C{}", c);
    }

    std::string pixel_preview_channel_label(const LoadedImage& image, int c)
    {
        if (c >= 0 && c < static_cast<int>(image.channel_names.size())
            && !image.channel_names[c].empty()) {
            return image.channel_names[c];
        }
        return channel_label_for_index(c);
    }

    double probe_value_to_oiio_float(UploadDataType type, double value)
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

    bool sample_loaded_pixel_with_semantics(const LoadedImage& image, int x,
                                            int y,
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

    bool compute_area_stats(
        const LoadedImage& image, int center_x, int center_y, int window_size,
        std::vector<double>& out_min, std::vector<double>& out_max,
        std::vector<double>& out_avg, int& out_samples,
        ProbeStatsSemantics semantics = ProbeStatsSemantics::RawStored)
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
        const int x1 = std::min(image.width - 1, center_x + half_window);
        const int y0 = std::max(0, center_y - half_window);
        const int y1 = std::min(image.height - 1, center_y + half_window);
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
                                                        sample)) {
                    continue;
                }
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

    bool compute_rect_stats(const LoadedImage& image, int xbegin, int ybegin,
                            int xend, int yend, std::vector<double>& out_min,
                            std::vector<double>& out_max,
                            std::vector<double>& out_avg, int& out_samples,
                            ProbeStatsSemantics semantics
                            = ProbeStatsSemantics::OIIOFloat)
    {
        out_min.clear();
        out_max.clear();
        out_avg.clear();
        out_samples = 0;
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0)
            return false;

        const int xmin = std::clamp(std::min(xbegin, xend), 0, image.width - 1);
        const int xmax = std::clamp(std::max(xbegin, xend), 0, image.width - 1);
        const int ymin = std::clamp(std::min(ybegin, yend), 0,
                                    image.height - 1);
        const int ymax = std::clamp(std::max(ybegin, yend), 0,
                                    image.height - 1);
        if (xmax < xmin || ymax < ymin)
            return false;

        const size_t channels = static_cast<size_t>(image.nchannels);
        out_min.assign(channels, std::numeric_limits<double>::infinity());
        out_max.assign(channels, -std::numeric_limits<double>::infinity());
        out_avg.assign(channels, 0.0);

        std::vector<double> sample;
        for (int y = ymin; y <= ymax; ++y) {
            for (int x = xmin; x <= xmax; ++x) {
                if (!sample_loaded_pixel_with_semantics(image, x, y, semantics,
                                                        sample)) {
                    continue;
                }
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

    void build_area_probe_placeholder_lines(const LoadedImage& image,
                                            std::vector<std::string>& out_lines)
    {
        out_lines.clear();
        out_lines.emplace_back("Area Probe:");
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0)
            return;
        for (int c = 0; c < image.nchannels; ++c) {
            const std::string channel
                = pixel_preview_channel_label(image, static_cast<int>(c));
            out_lines.emplace_back(Strutil::fmt::format(
                "{:<5}: [min:  -----  max:  -----  avg:  -----]", channel));
        }
    }

    void build_area_probe_result_lines(const LoadedImage& image,
                                       const std::vector<double>& min_values,
                                       const std::vector<double>& max_values,
                                       const std::vector<double>& avg_values,
                                       std::vector<std::string>& out_lines)
    {
        out_lines.clear();
        out_lines.emplace_back("Area Probe:");
        const int channel_count = std::max(1, image.nchannels);
        for (int c = 0; c < channel_count; ++c) {
            const std::string channel
                = pixel_preview_channel_label(image, static_cast<int>(c));
            if (static_cast<size_t>(c) < min_values.size()
                && static_cast<size_t>(c) < max_values.size()
                && static_cast<size_t>(c) < avg_values.size()) {
                out_lines.emplace_back(Strutil::fmt::format(
                    "{:<5}: [min: {:>6.3f}  max: {:>6.3f}  avg: {:>6.3f}]",
                    channel, min_values[c], max_values[c], avg_values[c]));
            } else {
                out_lines.emplace_back(Strutil::fmt::format(
                    "{:<5}: [min:  -----  max:  -----  avg:  -----]", channel));
            }
        }
    }

    std::string format_probe_display_value(double value)
    {
        if (!std::isfinite(value))
            return Strutil::fmt::format("{:>8}", value);

        const double abs_value = std::abs(value);
        if ((abs_value > 99999.0) || (abs_value > 0.0 && abs_value < 0.00001))
            return Strutil::fmt::format("{: .1e}", value);

        int digits_before_decimal = 1;
        if (abs_value >= 1.0) {
            digits_before_decimal
                = static_cast<int>(std::floor(std::log10(abs_value))) + 1;
        }
        digits_before_decimal = std::clamp(digits_before_decimal, 1, 5);
        const int decimals    = std::clamp(6 - digits_before_decimal, 1, 5);

        const std::string format = Strutil::fmt::format("{{: .{}f}}", decimals);
        return Strutil::fmt::format(format, value);
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
            const ImVec2 size
                = draw_font->CalcTextSizeA(font_size,
                                           std::numeric_limits<float>::max(),
                                           0.0f, line.c_str());
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

    void draw_corner_marker(ImDrawList* draw_list, const ImVec2& p0,
                            const ImVec2& p1, ImU32 color)
    {
        const float corner_size = 4.0f;
        draw_list->AddLine(p0, ImVec2(p0.x + corner_size, p0.y), color, 1.0f);
        draw_list->AddLine(p0, ImVec2(p0.x, p0.y + corner_size), color, 1.0f);
        draw_list->AddLine(ImVec2(p1.x - corner_size, p0.y), ImVec2(p1.x, p0.y),
                           color, 1.0f);
        draw_list->AddLine(ImVec2(p1.x, p0.y), ImVec2(p1.x, p0.y + corner_size),
                           color, 1.0f);
        draw_list->AddLine(ImVec2(p0.x, p1.y - corner_size), ImVec2(p0.x, p1.y),
                           color, 1.0f);
        draw_list->AddLine(ImVec2(p0.x, p1.y), ImVec2(p0.x + corner_size, p1.y),
                           color, 1.0f);
        draw_list->AddLine(ImVec2(p1.x - corner_size, p1.y), p1, color, 1.0f);
        draw_list->AddLine(ImVec2(p1.x, p1.y - corner_size), p1, color, 1.0f);
    }

    bool image_selection_rect_to_screen(const ViewerState& viewer,
                                        const ImageCoordinateMap& map,
                                        ImVec2& out_min, ImVec2& out_max)
    {
        if (!has_image_selection(viewer) || !map.valid
            || viewer.image.width <= 0 || viewer.image.height <= 0) {
            return false;
        }

        const float x0 = static_cast<float>(viewer.selection_xbegin)
                         / static_cast<float>(viewer.image.width);
        const float x1 = static_cast<float>(viewer.selection_xend)
                         / static_cast<float>(viewer.image.width);
        const float y0 = static_cast<float>(viewer.selection_ybegin)
                         / static_cast<float>(viewer.image.height);
        const float y1 = static_cast<float>(viewer.selection_yend)
                         / static_cast<float>(viewer.image.height);

        const ImVec2 corners_uv[] = { ImVec2(x0, y0), ImVec2(x1, y0),
                                      ImVec2(x1, y1), ImVec2(x0, y1) };
        ImVec2 screen_corners[4];
        for (int i = 0; i < 4; ++i) {
            if (!source_uv_to_screen(map, corners_uv[i], screen_corners[i]))
                return false;
        }

        out_min = screen_corners[0];
        out_max = screen_corners[0];
        for (int i = 1; i < 4; ++i) {
            out_min.x = std::min(out_min.x, screen_corners[i].x);
            out_min.y = std::min(out_min.y, screen_corners[i].y);
            out_max.x = std::max(out_max.x, screen_corners[i].x);
            out_max.y = std::max(out_max.y, screen_corners[i].y);
        }
        return true;
    }

    const char* channel_view_name(int mode)
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

    const char* color_mode_name(int mode)
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

    const char* mouse_mode_name(int mode)
    {
        switch (mode) {
        case 0: return "Navigate";
        case 1: return "Pan";
        case 2: return "Wipe";
        case 3: return "Area Sample";
        case 4: return "Annotate";
        default: break;
        }
        return "Navigate";
    }

    std::string upper_ascii_copy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return value;
    }

    std::string status_channel_layout_text(const LoadedImage& image)
    {
        switch (image.nchannels) {
        case 1: return "GRAY";
        case 2: return "RG";
        case 3: return "RGB";
        case 4: return "RGBA";
        default: break;
        }
        return Strutil::fmt::format("{}CH", std::max(0, image.nchannels));
    }

    std::string status_image_text(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "No image loaded";
        const std::filesystem::path path(viewer.image.path);
        const std::string filename = path.filename().empty()
                                         ? viewer.image.path
                                         : path.filename().string();
        return filename;
    }

    std::string status_specs_text(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "";

        std::string type_name = viewer.image.data_format_name;
        if (type_name.empty()) {
            type_name = std::string(
                upload_data_type_to_typedesc(viewer.image.type).c_str());
        }
        if (type_name.empty() || type_name == "unknown")
            type_name = std::string(upload_data_type_name(viewer.image.type));
        type_name = upper_ascii_copy(type_name);

        return Strutil::fmt::format("{}x{}  {}  {}", viewer.image.width,
                                    viewer.image.height,
                                    status_channel_layout_text(viewer.image),
                                    type_name);
    }

    std::string status_preview_text(const ViewerState& viewer,
                                    const PlaceholderUiState& ui)
    {
        if (viewer.image.path.empty())
            return "";

        const float zoom  = std::max(viewer.zoom, 0.00001f);
        const float z_num = zoom >= 1.0f ? zoom : 1.0f;
        const float z_den = zoom >= 1.0f ? 1.0f : (1.0f / zoom);
        std::string text  = Strutil::fmt::format(
            "zoom {:.2f}:{:.2f}  exp {:+.1f}  gam {:.2f}  shift {:+.2f}", z_num,
            z_den, ui.exposure, ui.gamma, ui.offset);
        if (ui.color_mode != 0 || ui.current_channel != 0) {
            std::string mode = color_mode_name(ui.color_mode);
            if (ui.color_mode == 2 || ui.color_mode == 4) {
                mode += Strutil::fmt::format(" {}", ui.current_channel);
            } else {
                mode += Strutil::fmt::format(" ({})", channel_view_name(
                                                          ui.current_channel));
            }
            text += Strutil::fmt::format("  view {}", mode);
        }
        if (viewer.image.nsubimages > 1) {
            if (viewer.auto_subimage) {
                text += Strutil::fmt::format("  subimg AUTO ({}/{})",
                                             viewer.image.subimage + 1,
                                             viewer.image.nsubimages);
            } else {
                text += Strutil::fmt::format("  subimg {}/{}",
                                             viewer.image.subimage + 1,
                                             viewer.image.nsubimages);
            }
        }
        if (viewer.image.nmiplevels > 1) {
            text += Strutil::fmt::format("  MIP {}/{}",
                                         viewer.image.miplevel + 1,
                                         viewer.image.nmiplevels);
        }
        if (viewer.image.orientation != 1) {
            text += Strutil::fmt::format("  orient {}",
                                         viewer.image.orientation);
        }
        if (ui.show_mouse_mode_selector) {
            text += Strutil::fmt::format("  mouse {}",
                                         mouse_mode_name(ui.mouse_mode));
        }
        return text;
    }

}  // namespace

void
reset_area_probe_overlay(ViewerState& viewer)
{
    viewer.area_probe_drag_active   = false;
    viewer.area_probe_drag_start_uv = ImVec2(0.0f, 0.0f);
    viewer.area_probe_drag_end_uv   = ImVec2(0.0f, 0.0f);
    build_area_probe_placeholder_lines(viewer.image, viewer.area_probe_lines);
}

void
update_area_probe_overlay(ViewerState& viewer, int xbegin, int ybegin, int xend,
                          int yend)
{
    if (viewer.image.path.empty()) {
        viewer.area_probe_lines.clear();
        return;
    }

    std::vector<double> min_values;
    std::vector<double> max_values;
    std::vector<double> avg_values;
    int sample_count = 0;
    if (!compute_rect_stats(viewer.image, xbegin, ybegin, xend, yend,
                            min_values, max_values, avg_values, sample_count,
                            ProbeStatsSemantics::OIIOFloat)) {
        build_area_probe_placeholder_lines(viewer.image,
                                           viewer.area_probe_lines);
        return;
    }

    build_area_probe_result_lines(viewer.image, min_values, max_values,
                                  avg_values, viewer.area_probe_lines);
}

void
sync_area_probe_to_selection(ViewerState& viewer,
                             const PlaceholderUiState& ui_state)
{
    if (!ui_state.show_area_probe_window || !has_image_selection(viewer)) {
        reset_area_probe_overlay(viewer);
        return;
    }

    update_area_probe_overlay(viewer, viewer.selection_xbegin,
                              viewer.selection_ybegin,
                              viewer.selection_xend - 1,
                              viewer.selection_yend - 1);
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
    if (ui_state.show_area_probe_window && viewer.area_probe_drag_active)
        return panel;

    std::vector<std::string> lines;
    std::vector<ImU32> line_colors;
    if (viewer.image.path.empty()) {
        lines.emplace_back("No image loaded.");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
    } else if (!viewer.probe_valid) {
        lines.emplace_back("Hover over image to inspect.");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
    } else {
        lines.emplace_back(Strutil::fmt::format("X:  {:>7}       Y:  {:>7}",
                                                viewer.probe_x,
                                                viewer.probe_y));
        line_colors.emplace_back(IM_COL32(0, 255, 255, 220));

        std::vector<double> min_values;
        std::vector<double> max_values;
        std::vector<double> avg_values;
        int sample_count = 0;
        const bool have_stats
            = compute_area_stats(viewer.image, viewer.probe_x, viewer.probe_y,
                                 ui_state.closeup_avg_pixels, min_values,
                                 max_values, avg_values, sample_count,
                                 ProbeStatsSemantics::OIIOFloat);

        lines.emplace_back("");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
        lines.emplace_back(Strutil::fmt::format("{:<2} {:>8} {:>8} {:>8} {:>8}",
                                                "", "Val", "Min", "Max",
                                                "Avg"));
        line_colors.emplace_back(IM_COL32(255, 255, 160, 220));
        for (size_t c = 0; c < viewer.probe_channels.size(); ++c) {
            const std::string label
                = pixel_preview_channel_label(viewer.image,
                                              static_cast<int>(c));
            const double semantic_value
                = probe_value_to_oiio_float(viewer.image.type,
                                            viewer.probe_channels[c]);
            const std::string value = format_probe_display_value(
                semantic_value);
            if (have_stats && c < min_values.size() && c < max_values.size()
                && c < avg_values.size()) {
                lines.emplace_back(Strutil::fmt::format(
                    "{:<2} {:>8} {:>8} {:>8} {:>8}", label, value,
                    format_probe_display_value(min_values[c]),
                    format_probe_display_value(max_values[c]),
                    format_probe_display_value(avg_values[c])));
            } else {
                lines.emplace_back(
                    Strutil::fmt::format("{:<2} {:>8} {:>8} {:>8} {:>8}", label,
                                         value, "-----", "-----", "-----"));
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
    const float text_font_size = 13.5f;

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

        const int center_ix = center_x - xbegin;
        const int center_iy = center_y - ybegin;
        const ImVec2 center_min(closeup_min.x + center_ix * cell_w,
                                closeup_min.y + center_iy * cell_h);
        const ImVec2 center_max(center_min.x + cell_w, center_min.y + cell_h);
        draw_corner_marker(draw_list, center_min, center_max,
                           IM_COL32(0, 255, 255, 180));

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
            draw_corner_marker(draw_list, avg_min, avg_max,
                               IM_COL32(255, 255, 0, 170));
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
    register_layout_dump_synthetic_rect("text", "Pixel Closeup overlay",
                                        panel.min, panel.max);
    return panel;
}

void
draw_image_selection_overlay(const ViewerState& viewer,
                             const ImageCoordinateMap& map)
{
    if (!map.valid)
        return;

    ImVec2 rect_min(0.0f, 0.0f);
    ImVec2 rect_max(0.0f, 0.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(map.viewport_rect_min, map.viewport_rect_max, true);
    if (viewer.selection_drag_active) {
        ImVec2 start_screen(0.0f, 0.0f);
        ImVec2 end_screen(0.0f, 0.0f);
        if (source_uv_to_screen(map, viewer.selection_drag_start_uv,
                                start_screen)
            && source_uv_to_screen(map, viewer.selection_drag_end_uv,
                                   end_screen)) {
            rect_min = ImVec2(std::min(start_screen.x, end_screen.x),
                              std::min(start_screen.y, end_screen.y));
            rect_max = ImVec2(std::max(start_screen.x, end_screen.x),
                              std::max(start_screen.y, end_screen.y));
            draw_list->AddRectFilled(rect_min, rect_max,
                                     IM_COL32(72, 196, 255, 42), 0.0f);
            draw_list->AddRect(rect_min, rect_max, IM_COL32(72, 196, 255, 255),
                               0.0f, 0, 1.2f);
        }
    } else if (image_selection_rect_to_screen(viewer, map, rect_min, rect_max)) {
        draw_list->AddRect(rect_min, rect_max, IM_COL32(72, 196, 255, 255),
                           0.0f, 0, 1.2f);
    } else {
        draw_list->PopClipRect();
        return;
    }
    draw_list->PopClipRect();
    register_layout_dump_synthetic_rect("rect", "Image selection overlay",
                                        rect_min, rect_max);
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

    std::vector<std::string> lines = viewer.area_probe_lines;
    if (lines.empty()) {
        if (viewer.image.path.empty()) {
            lines.emplace_back("Area Probe:");
            lines.emplace_back("No image loaded.");
        } else {
            build_area_probe_placeholder_lines(viewer.image, lines);
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
    const OverlayPanelRect panel
        = draw_overlay_text_panel(lines, preferred, map.viewport_rect_min,
                                  map.viewport_rect_max, fonts.mono);
    if (panel.valid) {
        register_layout_dump_synthetic_rect("text", "Area Probe overlay",
                                            panel.min, panel.max);
    }
}

void
draw_embedded_status_bar(ViewerState& viewer, PlaceholderUiState& ui)
{
    const std::string filename_text = status_image_text(viewer);
    const std::string specs_text    = status_specs_text(viewer);
    const std::string preview_text  = status_preview_text(viewer, ui);
    const bool show_progress        = false;

    int columns = 3;
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
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch,
                                2.2f);
        ImGui::TableSetupColumn("Specs", ImGuiTableColumnFlags_WidthStretch,
                                1.8f);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                2.2f);
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
        ImGui::TextUnformatted(filename_text.c_str());
        register_layout_dump_synthetic_item("text", filename_text.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(specs_text.c_str());
        register_layout_dump_synthetic_item("text", specs_text.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(preview_text.c_str());
        register_layout_dump_synthetic_item("text", preview_text.c_str());

        if (show_progress) {
            ImGui::TableNextColumn();
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "idle");
        }

        if (ui.show_mouse_mode_selector) {
            ImGui::TableNextColumn();
            if (ui.show_area_probe_window) {
                ImGui::TextUnformatted("Area Sample");
                register_layout_dump_synthetic_item("text", "Area Sample");
            } else {
                static const char* mouse_modes[] = { "Navigate", "Pan", "Wipe",
                                                     "Annotate" };
                static const int mouse_mode_values[] = { 0, 1, 2, 4 };
                int combo_index                      = 0;
                for (int i = 0; i < IM_ARRAYSIZE(mouse_mode_values); ++i) {
                    if (ui.mouse_mode == mouse_mode_values[i]) {
                        combo_index = i;
                        break;
                    }
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##mouse_mode", &combo_index, mouse_modes,
                                 IM_ARRAYSIZE(mouse_modes))) {
                    set_mouse_mode_action(viewer, ui,
                                          mouse_mode_values[combo_index]);
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

}  // namespace Imiv
