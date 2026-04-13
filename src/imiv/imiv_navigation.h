// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

namespace Imiv {

struct ImageCoordinateMap {
    bool valid               = false;
    int source_width         = 0;
    int source_height        = 0;
    int orientation          = 1;
    ImVec2 image_rect_min    = ImVec2(0.0f, 0.0f);
    ImVec2 image_rect_max    = ImVec2(0.0f, 0.0f);
    ImVec2 viewport_rect_min = ImVec2(0.0f, 0.0f);
    ImVec2 viewport_rect_max = ImVec2(0.0f, 0.0f);
    ImVec2 window_pos        = ImVec2(0.0f, 0.0f);
};

struct ImageViewportLayout {
    ImVec2 child_size   = ImVec2(0.0f, 0.0f);
    ImVec2 inner_size   = ImVec2(0.0f, 0.0f);
    ImVec2 content_size = ImVec2(0.0f, 0.0f);
    ImVec2 image_size   = ImVec2(0.0f, 0.0f);
    bool scroll_x       = false;
    bool scroll_y       = false;
};

struct PendingZoomRequest {
    float scale       = 1.0f;
    bool snap_to_one  = false;
    bool prefer_mouse = false;
};

ImVec2
source_uv_to_display_uv(const ImVec2& src_uv, int orientation);
ImVec2
display_uv_to_source_uv(const ImVec2& display_uv, int orientation);
ImVec2
screen_to_window_coords(const ImageCoordinateMap& map,
                        const ImVec2& screen_pos);
ImVec2
window_to_screen_coords(const ImageCoordinateMap& map,
                        const ImVec2& window_pos);
bool
screen_to_display_uv(const ImageCoordinateMap& map, const ImVec2& screen_pos,
                     ImVec2& out_display_uv);
bool
screen_to_source_uv(const ImageCoordinateMap& map, const ImVec2& screen_pos,
                    ImVec2& out_source_uv);
bool
source_uv_to_screen(const ImageCoordinateMap& map, const ImVec2& source_uv,
                    ImVec2& out_screen_pos);
bool
point_in_rect(const ImVec2& pos, const ImVec2& rect_min,
              const ImVec2& rect_max);
ImVec2
clamp_pos_to_rect(const ImVec2& pos, const ImVec2& rect_min,
                  const ImVec2& rect_max);
ImVec2
rect_center(const ImVec2& rect_min, const ImVec2& rect_max);
ImVec2
rect_size(const ImVec2& rect_min, const ImVec2& rect_max);
bool
viewport_axis_needs_scroll(float image_axis, float inner_axis);
ImageViewportLayout
compute_image_viewport_layout(const ImVec2& child_size, const ImVec2& padding,
                              const ImVec2& image_size, float scrollbar_size);
void
sync_view_scroll_from_display_scroll(ViewerState& viewer,
                                     const ImVec2& display_scroll,
                                     const ImVec2& image_size);
void
sync_view_scroll_from_source_uv(ViewerState& viewer, const ImVec2& source_uv,
                                int orientation, const ImVec2& image_size);
void
queue_zoom_pivot(ViewerState& viewer, const ImVec2& anchor_screen,
                 const ImVec2& source_uv);
void
request_zoom_scale(PendingZoomRequest& request, float scale, bool prefer_mouse);
void
request_zoom_reset(PendingZoomRequest& request, bool prefer_mouse);
void
recenter_view(ViewerState& viewer, const ImVec2& image_size);
float
compute_fit_zoom(const ImVec2& child_size, const ImVec2& padding,
                 int display_width, int display_height);
void
compute_zoom_pivot(const ImageCoordinateMap& map, const ImVec2& mouse_screen,
                   bool prefer_mouse_position, ImVec2& out_anchor_screen,
                   ImVec2& out_source_uv);
bool
apply_zoom_request(const ImageCoordinateMap& map, ViewerState& viewer,
                   PlaceholderUiState& ui_state,
                   const PendingZoomRequest& request,
                   const ImVec2& mouse_screen);
void
apply_pending_zoom_pivot(ViewerState& viewer, const ImageCoordinateMap& map,
                         const ImVec2& image_size, bool can_scroll_x,
                         bool can_scroll_y);
bool
source_uv_to_pixel(const ImageCoordinateMap& map, const ImVec2& source_uv,
                   int& out_px, int& out_py);

}  // namespace Imiv
