// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_probe_overlay.h"

#include "imiv_probe_data.h"
#include "imiv_test_engine.h"
#include "imiv_ui_metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

namespace {

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
        const float pad_x     = UiMetrics::OverlayPanel::kPadX;
        const float pad_y     = UiMetrics::OverlayPanel::kPadY;
        const float line_gap  = UiMetrics::OverlayPanel::kLineGap;
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
                                 IM_COL32(20, 24, 30, 224),
                                 UiMetrics::OverlayPanel::kCornerRounding);
        draw_list->AddRect(pos, ImVec2(pos.x + panel_w, pos.y + panel_h),
                           IM_COL32(175, 185, 205, 255),
                           UiMetrics::OverlayPanel::kCornerRounding, 0,
                           UiMetrics::OverlayPanel::kBorderThickness);

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
        const float corner_size = UiMetrics::OverlayPanel::kCornerMarkerSize;
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

}  // namespace

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

    ProbeOverlayText text;
    build_pixel_closeup_overlay_text(viewer, ui_state, text);
    const std::vector<std::string>& lines = text.lines;
    const std::vector<ImU32>& line_colors = text.colors;

    const float closeup_window_size = UiMetrics::PixelCloseup::kWindowSize;
    const float follow_mouse_offset
        = UiMetrics::PixelCloseup::kFollowMouseOffset;
    const float corner_padding     = UiMetrics::PixelCloseup::kCornerPadding;
    const float text_pad_x         = UiMetrics::PixelCloseup::kTextPadX;
    const float text_pad_y         = UiMetrics::PixelCloseup::kTextPadY;
    const float text_line_gap      = UiMetrics::PixelCloseup::kTextLineGap;
    const float text_to_window_gap = UiMetrics::PixelCloseup::kTextToWindowGap;
    const float text_wrap_w        = std::max(8.0f,
                                              closeup_window_size - text_pad_x * 2.0f);
    ImFont* text_font              = fonts.mono ? fonts.mono : ImGui::GetFont();
    const float text_font_size     = UiMetrics::PixelCloseup::kFontSize;

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
                             IM_COL32(20, 24, 30, 224),
                             UiMetrics::OverlayPanel::kCornerRounding);
    draw_list->AddRect(closeup_min, closeup_max, IM_COL32(175, 185, 205, 255),
                       UiMetrics::OverlayPanel::kCornerRounding, 0,
                       UiMetrics::OverlayPanel::kBorderThickness);

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
                             UiMetrics::OverlayPanel::kCornerRounding);
    draw_list->AddRect(text_min, text_max, IM_COL32(175, 185, 205, 255),
                       UiMetrics::OverlayPanel::kCornerRounding, 0,
                       UiMetrics::OverlayPanel::kBorderThickness);
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
    build_area_probe_overlay_lines(viewer, lines);

    const float clip_min_x  = std::min(map.viewport_rect_min.x,
                                       map.viewport_rect_max.x);
    const float clip_max_y  = std::max(map.viewport_rect_min.y,
                                       map.viewport_rect_max.y);
    const float pad_y       = UiMetrics::AreaProbe::kPadY;
    const float line_gap    = UiMetrics::AreaProbe::kLineGap;
    const ImFont* mono_font = fonts.mono ? fonts.mono : ImGui::GetFont();
    const float line_h      = mono_font ? mono_font->LegacySize
                                        : ImGui::GetTextLineHeight();
    float panel_h = pad_y * 2.0f + static_cast<float>(lines.size()) * line_h
                    + static_cast<float>(std::max<size_t>(0, lines.size() - 1))
                          * line_gap;
    const float border_margin = UiMetrics::AreaProbe::kBorderMargin;
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

}  // namespace Imiv
