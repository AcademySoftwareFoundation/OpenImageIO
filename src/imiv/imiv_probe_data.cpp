// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_probe_data.h"

#include "imiv_probe_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

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

    bool compute_region_stats(const LoadedImage& image, int xmin, int ymin,
                              int xmax, int ymax, std::vector<double>& out_min,
                              std::vector<double>& out_max,
                              std::vector<double>& out_avg, int& out_samples,
                              ProbeStatsSemantics semantics)
    {
        out_min.clear();
        out_max.clear();
        out_avg.clear();
        out_samples = 0;
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0)
            return false;
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

    bool compute_area_stats(
        const LoadedImage& image, int center_x, int center_y, int window_size,
        std::vector<double>& out_min, std::vector<double>& out_max,
        std::vector<double>& out_avg, int& out_samples,
        ProbeStatsSemantics semantics = ProbeStatsSemantics::RawStored)
    {
        if (window_size <= 0)
            return false;
        if ((window_size & 1) == 0)
            ++window_size;
        if (image.width <= 0 || image.height <= 0)
            return false;

        const int half_window = window_size / 2;
        const int x0          = std::max(0, center_x - half_window);
        const int x1 = std::min(image.width - 1, center_x + half_window);
        const int y0 = std::max(0, center_y - half_window);
        const int y1 = std::min(image.height - 1, center_y + half_window);
        return compute_region_stats(image, x0, y0, x1, y1, out_min, out_max,
                                    out_avg, out_samples, semantics);
    }

    bool compute_rect_stats(const LoadedImage& image, int xbegin, int ybegin,
                            int xend, int yend, std::vector<double>& out_min,
                            std::vector<double>& out_max,
                            std::vector<double>& out_avg, int& out_samples,
                            ProbeStatsSemantics semantics
                            = ProbeStatsSemantics::OIIOFloat)
    {
        if (image.width <= 0 || image.height <= 0)
            return false;
        const int xmin = std::clamp(std::min(xbegin, xend), 0, image.width - 1);
        const int xmax = std::clamp(std::max(xbegin, xend), 0, image.width - 1);
        const int ymin = std::clamp(std::min(ybegin, yend), 0,
                                    image.height - 1);
        const int ymax = std::clamp(std::max(ybegin, yend), 0,
                                    image.height - 1);
        return compute_region_stats(image, xmin, ymin, xmax, ymax, out_min,
                                    out_max, out_avg, out_samples, semantics);
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

    ImU32 probe_channel_color(const std::string& label)
    {
        if (!label.empty()) {
            if (label[0] == 'R')
                return IM_COL32(250, 94, 143, 255);
            if (label[0] == 'G')
                return IM_COL32(135, 203, 124, 255);
            if (label[0] == 'B')
                return IM_COL32(107, 188, 255, 255);
        }
        return IM_COL32(220, 220, 220, 255);
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

void
build_area_probe_overlay_lines(const ViewerState& viewer,
                               std::vector<std::string>& out_lines)
{
    out_lines = viewer.area_probe_lines;
    if (!out_lines.empty())
        return;

    if (viewer.image.path.empty()) {
        out_lines.emplace_back("Area Probe:");
        out_lines.emplace_back("No image loaded.");
        return;
    }

    build_area_probe_placeholder_lines(viewer.image, out_lines);
}

void
build_pixel_closeup_overlay_text(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ProbeOverlayText& out_text)
{
    out_text.lines.clear();
    out_text.colors.clear();

    if (viewer.image.path.empty()) {
        out_text.lines.emplace_back("No image loaded.");
        out_text.colors.emplace_back(IM_COL32(240, 242, 245, 255));
        return;
    }
    if (!viewer.probe_valid) {
        out_text.lines.emplace_back("Hover over image to inspect.");
        out_text.colors.emplace_back(IM_COL32(240, 242, 245, 255));
        return;
    }

    out_text.lines.reserve(viewer.probe_channels.size() + 3);
    out_text.colors.reserve(viewer.probe_channels.size() + 3);

    out_text.lines.emplace_back(
        Strutil::fmt::format("X:  {:>7}       Y:  {:>7}", viewer.probe_x,
                             viewer.probe_y));
    out_text.colors.emplace_back(IM_COL32(0, 255, 255, 220));

    std::vector<double> min_values;
    std::vector<double> max_values;
    std::vector<double> avg_values;
    int sample_count = 0;
    const bool have_stats
        = compute_area_stats(viewer.image, viewer.probe_x, viewer.probe_y,
                             ui_state.closeup_avg_pixels, min_values,
                             max_values, avg_values, sample_count,
                             ProbeStatsSemantics::OIIOFloat);

    out_text.lines.emplace_back("");
    out_text.colors.emplace_back(IM_COL32(240, 242, 245, 255));
    out_text.lines.emplace_back(
        Strutil::fmt::format("{:<2} {:>8} {:>8} {:>8} {:>8}", "", "Val", "Min",
                             "Max", "Avg"));
    out_text.colors.emplace_back(IM_COL32(255, 255, 160, 220));
    for (size_t c = 0; c < viewer.probe_channels.size(); ++c) {
        const std::string label
            = pixel_preview_channel_label(viewer.image, static_cast<int>(c));
        const double semantic_value
            = probe_value_to_oiio_float(viewer.image.type,
                                        viewer.probe_channels[c]);
        const std::string value = format_probe_display_value(semantic_value);
        if (have_stats && c < min_values.size() && c < max_values.size()
            && c < avg_values.size()) {
            out_text.lines.emplace_back(Strutil::fmt::format(
                "{:<2} {:>8} {:>8} {:>8} {:>8}", label, value,
                format_probe_display_value(min_values[c]),
                format_probe_display_value(max_values[c]),
                format_probe_display_value(avg_values[c])));
        } else {
            out_text.lines.emplace_back(
                Strutil::fmt::format("{:<2} {:>8} {:>8} {:>8} {:>8}", label,
                                     value, "-----", "-----", "-----"));
        }
        out_text.colors.emplace_back(probe_channel_color(label));
    }
    (void)sample_count;
}

}  // namespace Imiv
