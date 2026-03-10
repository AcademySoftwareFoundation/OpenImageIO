// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_navigation.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace Imiv {

ImVec2
source_uv_to_display_uv(const ImVec2& src_uv, int orientation)
{
    const float u = src_uv.x;
    const float v = src_uv.y;
    switch (clamp_orientation(orientation)) {
    case 1: return ImVec2(u, v);
    case 2: return ImVec2(1.0f - u, v);
    case 3: return ImVec2(1.0f - u, 1.0f - v);
    case 4: return ImVec2(u, 1.0f - v);
    case 5: return ImVec2(v, u);
    case 6: return ImVec2(1.0f - v, u);
    case 7: return ImVec2(1.0f - v, 1.0f - u);
    case 8: return ImVec2(v, 1.0f - u);
    default: break;
    }
    return ImVec2(u, v);
}

ImVec2
display_uv_to_source_uv(const ImVec2& display_uv, int orientation)
{
    const float u = display_uv.x;
    const float v = display_uv.y;
    switch (clamp_orientation(orientation)) {
    case 1: return ImVec2(u, v);
    case 2: return ImVec2(1.0f - u, v);
    case 3: return ImVec2(1.0f - u, 1.0f - v);
    case 4: return ImVec2(u, 1.0f - v);
    case 5: return ImVec2(v, u);
    case 6: return ImVec2(v, 1.0f - u);
    case 7: return ImVec2(1.0f - v, 1.0f - u);
    case 8: return ImVec2(1.0f - v, u);
    default: break;
    }
    return ImVec2(u, v);
}

ImVec2
screen_to_window_coords(const ImageCoordinateMap& map, const ImVec2& screen_pos)
{
    return ImVec2(screen_pos.x - map.window_pos.x,
                  screen_pos.y - map.window_pos.y);
}

ImVec2
window_to_screen_coords(const ImageCoordinateMap& map, const ImVec2& window_pos)
{
    return ImVec2(window_pos.x + map.window_pos.x,
                  window_pos.y + map.window_pos.y);
}

bool
screen_to_display_uv(const ImageCoordinateMap& map, const ImVec2& screen_pos,
                     ImVec2& out_display_uv)
{
    out_display_uv = ImVec2(0.0f, 0.0f);
    if (!map.valid)
        return false;
    const float w = map.image_rect_max.x - map.image_rect_min.x;
    const float h = map.image_rect_max.y - map.image_rect_min.y;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    const float u  = (screen_pos.x - map.image_rect_min.x) / w;
    const float v  = (screen_pos.y - map.image_rect_min.y) / h;
    out_display_uv = ImVec2(u, v);
    return (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
}

bool
screen_to_source_uv(const ImageCoordinateMap& map, const ImVec2& screen_pos,
                    ImVec2& out_source_uv)
{
    ImVec2 display_uv(0.0f, 0.0f);
    if (!screen_to_display_uv(map, screen_pos, display_uv))
        return false;
    out_source_uv   = display_uv_to_source_uv(display_uv, map.orientation);
    out_source_uv.x = std::clamp(out_source_uv.x, 0.0f, 1.0f);
    out_source_uv.y = std::clamp(out_source_uv.y, 0.0f, 1.0f);
    return true;
}

bool
source_uv_to_screen(const ImageCoordinateMap& map, const ImVec2& source_uv,
                    ImVec2& out_screen_pos)
{
    out_screen_pos = ImVec2(0.0f, 0.0f);
    if (!map.valid)
        return false;
    const float w = map.image_rect_max.x - map.image_rect_min.x;
    const float h = map.image_rect_max.y - map.image_rect_min.y;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    const ImVec2 display_uv = source_uv_to_display_uv(source_uv,
                                                      map.orientation);
    out_screen_pos          = ImVec2(map.image_rect_min.x + display_uv.x * w,
                                     map.image_rect_min.y + display_uv.y * h);
    return true;
}

bool
point_in_rect(const ImVec2& pos, const ImVec2& rect_min, const ImVec2& rect_max)
{
    const float min_x = std::min(rect_min.x, rect_max.x);
    const float max_x = std::max(rect_min.x, rect_max.x);
    const float min_y = std::min(rect_min.y, rect_max.y);
    const float max_y = std::max(rect_min.y, rect_max.y);
    return pos.x >= min_x && pos.x <= max_x && pos.y >= min_y && pos.y <= max_y;
}

ImVec2
clamp_pos_to_rect(const ImVec2& pos, const ImVec2& rect_min,
                  const ImVec2& rect_max)
{
    const float min_x = std::min(rect_min.x, rect_max.x);
    const float max_x = std::max(rect_min.x, rect_max.x);
    const float min_y = std::min(rect_min.y, rect_max.y);
    const float max_y = std::max(rect_min.y, rect_max.y);
    return ImVec2(std::clamp(pos.x, min_x, max_x),
                  std::clamp(pos.y, min_y, max_y));
}

ImVec2
rect_center(const ImVec2& rect_min, const ImVec2& rect_max)
{
    return ImVec2((rect_min.x + rect_max.x) * 0.5f,
                  (rect_min.y + rect_max.y) * 0.5f);
}

ImVec2
rect_size(const ImVec2& rect_min, const ImVec2& rect_max)
{
    return ImVec2(std::abs(rect_max.x - rect_min.x),
                  std::abs(rect_max.y - rect_min.y));
}

bool
viewport_axis_needs_scroll(float image_axis, float inner_axis)
{
    return image_axis > inner_axis + 0.01f;
}

ImageViewportLayout
compute_image_viewport_layout(const ImVec2& child_size, const ImVec2& padding,
                              const ImVec2& image_size, float scrollbar_size)
{
    ImageViewportLayout layout;
    layout.child_size = child_size;
    layout.image_size = image_size;

    const ImVec2 base_inner(std::max(0.0f, child_size.x - padding.x * 2.0f),
                            std::max(0.0f, child_size.y - padding.y * 2.0f));
    bool scroll_x = viewport_axis_needs_scroll(image_size.x, base_inner.x);
    bool scroll_y = viewport_axis_needs_scroll(image_size.y, base_inner.y);
    for (int i = 0; i < 3; ++i) {
        const ImVec2 inner(
            std::max(0.0f, base_inner.x - (scroll_y ? scrollbar_size : 0.0f)),
            std::max(0.0f, base_inner.y - (scroll_x ? scrollbar_size : 0.0f)));
        const bool next_x = viewport_axis_needs_scroll(image_size.x, inner.x);
        const bool next_y = viewport_axis_needs_scroll(image_size.y, inner.y);
        layout.inner_size = inner;
        if (next_x == scroll_x && next_y == scroll_y)
            break;
        scroll_x = next_x;
        scroll_y = next_y;
    }
    layout.scroll_x       = scroll_x;
    layout.scroll_y       = scroll_y;
    layout.content_size.x = scroll_x ? (image_size.x + layout.inner_size.x)
                                     : layout.inner_size.x;
    layout.content_size.y = scroll_y ? (image_size.y + layout.inner_size.y)
                                     : layout.inner_size.y;
    return layout;
}

void
sync_view_scroll_from_display_scroll(ViewerState& viewer,
                                     const ImVec2& display_scroll,
                                     const ImVec2& image_size)
{
    viewer.max_scroll    = image_size;
    viewer.scroll.x      = std::clamp(display_scroll.x, 0.0f,
                                      std::max(0.0f, image_size.x));
    viewer.scroll.y      = std::clamp(display_scroll.y, 0.0f,
                                      std::max(0.0f, image_size.y));
    viewer.norm_scroll.x = (image_size.x > 0.0f)
                               ? (viewer.scroll.x / image_size.x)
                               : 0.5f;
    viewer.norm_scroll.y = (image_size.y > 0.0f)
                               ? (viewer.scroll.y / image_size.y)
                               : 0.5f;
}

void
sync_view_scroll_from_source_uv(ViewerState& viewer, const ImVec2& source_uv,
                                int orientation, const ImVec2& image_size)
{
    const ImVec2 display_uv
        = source_uv_to_display_uv(ImVec2(std::clamp(source_uv.x, 0.0f, 1.0f),
                                         std::clamp(source_uv.y, 0.0f, 1.0f)),
                                  orientation);
    viewer.max_scroll  = image_size;
    viewer.norm_scroll = display_uv;
    viewer.scroll      = ImVec2(display_uv.x * image_size.x,
                                display_uv.y * image_size.y);
}

void
queue_zoom_pivot(ViewerState& viewer, const ImVec2& anchor_screen,
                 const ImVec2& source_uv)
{
    viewer.zoom_pivot_screen      = anchor_screen;
    viewer.zoom_pivot_source_uv   = ImVec2(std::clamp(source_uv.x, 0.0f, 1.0f),
                                           std::clamp(source_uv.y, 0.0f, 1.0f));
    viewer.zoom_pivot_pending     = true;
    viewer.zoom_pivot_frames_left = 3;
}

void
request_zoom_scale(PendingZoomRequest& request, float scale, bool prefer_mouse)
{
    request.scale *= scale;
    request.prefer_mouse = request.prefer_mouse || prefer_mouse;
}

void
request_zoom_reset(PendingZoomRequest& request, bool prefer_mouse)
{
    request.snap_to_one  = true;
    request.prefer_mouse = request.prefer_mouse || prefer_mouse;
}

void
recenter_view(ViewerState& viewer, const ImVec2& image_size)
{
    viewer.zoom_pivot_pending     = false;
    viewer.zoom_pivot_frames_left = 0;
    sync_view_scroll_from_display_scroll(
        viewer, ImVec2(image_size.x * 0.5f, image_size.y * 0.5f), image_size);
    viewer.scroll_sync_frames_left = std::max(viewer.scroll_sync_frames_left,
                                              2);
}

float
compute_fit_zoom(const ImVec2& child_size, const ImVec2& padding,
                 int display_width, int display_height)
{
    if (display_width <= 0 || display_height <= 0)
        return 1.0f;

    const ImVec2 fit_inner(std::max(1.0f, child_size.x - padding.x * 2.0f),
                           std::max(1.0f, child_size.y - padding.y * 2.0f));
    const float fit_x = fit_inner.x / static_cast<float>(display_width);
    const float fit_y = fit_inner.y / static_cast<float>(display_height);
    if (!(fit_x > 0.0f && fit_y > 0.0f))
        return 1.0f;

    const float fit_zoom = std::min(fit_x, fit_y);
    return std::max(0.05f, std::nextafter(fit_zoom, 0.0f));
}

void
compute_zoom_pivot(const ImageCoordinateMap& map, const ImVec2& mouse_screen,
                   bool prefer_mouse_position, ImVec2& out_anchor_screen,
                   ImVec2& out_source_uv)
{
    out_anchor_screen = rect_center(map.viewport_rect_min,
                                    map.viewport_rect_max);
    if (prefer_mouse_position
        && point_in_rect(mouse_screen, map.viewport_rect_min,
                         map.viewport_rect_max)) {
        out_anchor_screen = mouse_screen;
    }

    ImVec2 sample_screen = out_anchor_screen;
    if (!point_in_rect(sample_screen, map.image_rect_min, map.image_rect_max)) {
        sample_screen = clamp_pos_to_rect(sample_screen, map.image_rect_min,
                                          map.image_rect_max);
    }

    if (!screen_to_source_uv(map, sample_screen, out_source_uv))
        out_source_uv = ImVec2(0.5f, 0.5f);
}

void
apply_zoom_request(const ImageCoordinateMap& map, ViewerState& viewer,
                   PlaceholderUiState& ui_state,
                   const PendingZoomRequest& request,
                   const ImVec2& mouse_screen)
{
    if (!map.valid)
        return;
    if (!request.snap_to_one && std::abs(request.scale - 1.0f) < 1.0e-6f)
        return;

    const float new_zoom = request.snap_to_one
                               ? 1.0f
                               : std::clamp(viewer.zoom * request.scale, 0.05f,
                                            64.0f);
    if (std::abs(new_zoom - viewer.zoom) < 1.0e-6f)
        return;

    ImVec2 anchor_screen(0.0f, 0.0f);
    ImVec2 source_uv(0.5f, 0.5f);
    compute_zoom_pivot(map, mouse_screen, request.prefer_mouse, anchor_screen,
                       source_uv);

    viewer.zoom                  = new_zoom;
    ui_state.fit_image_to_window = false;
    viewer.fit_request           = false;
    queue_zoom_pivot(viewer, anchor_screen, source_uv);
}

void
apply_pending_zoom_pivot(ViewerState& viewer, const ImageCoordinateMap& map,
                         const ImVec2& image_size, bool can_scroll_x,
                         bool can_scroll_y)
{
    if (!(viewer.zoom_pivot_pending || viewer.zoom_pivot_frames_left > 0))
        return;
    const ImVec2 viewport_center = rect_center(map.viewport_rect_min,
                                               map.viewport_rect_max);
    const ImVec2 display_uv
        = source_uv_to_display_uv(viewer.zoom_pivot_source_uv, map.orientation);
    const ImVec2 new_scroll((viewport_center.x - viewer.zoom_pivot_screen.x)
                                + display_uv.x * image_size.x,
                            (viewport_center.y - viewer.zoom_pivot_screen.y)
                                + display_uv.y * image_size.y);
    sync_view_scroll_from_display_scroll(viewer, new_scroll, image_size);
    if (can_scroll_x)
        ImGui::SetScrollX(viewer.scroll.x);
    else
        ImGui::SetScrollX(0.0f);
    if (can_scroll_y)
        ImGui::SetScrollY(viewer.scroll.y);
    else
        ImGui::SetScrollY(0.0f);
    viewer.zoom_pivot_pending = false;
    if (viewer.zoom_pivot_frames_left > 0)
        --viewer.zoom_pivot_frames_left;
}

bool
source_uv_to_pixel(const ImageCoordinateMap& map, const ImVec2& source_uv,
                   int& out_px, int& out_py)
{
    out_px = 0;
    out_py = 0;
    if (!map.valid || map.source_width <= 0 || map.source_height <= 0)
        return false;

    const float u = std::clamp(source_uv.x, 0.0f, 1.0f);
    const float v = std::clamp(source_uv.y, 0.0f, 1.0f);
    out_px        = std::clamp(static_cast<int>(std::floor(
                            u * static_cast<float>(map.source_width))),
                               0, map.source_width - 1);
    out_py        = std::clamp(static_cast<int>(std::floor(
                            v * static_cast<float>(map.source_height))),
                               0, map.source_height - 1);
    return true;
}



}  // namespace Imiv
