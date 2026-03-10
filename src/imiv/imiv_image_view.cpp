// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_image_view.h"

#include "imiv_test_engine.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <imgui.h>

namespace Imiv {

namespace {

    bool read_env_value(const char* name, std::string& out_value)
    {
        out_value.clear();
#if defined(_WIN32)
        char* value       = nullptr;
        size_t value_size = 0;
        errno_t err       = _dupenv_s(&value, &value_size, name);
        if (err != 0 || value == nullptr || value_size == 0) {
            if (value != nullptr)
                std::free(value);
            return false;
        }
        out_value.assign(value);
        std::free(value);
#else
        const char* value = std::getenv(name);
        if (value == nullptr)
            return false;
        out_value.assign(value);
#endif
        return true;
    }

    int env_int_value(const char* name, int fallback)
    {
        std::string value;
        if (!read_env_value(name, value) || value.empty())
            return fallback;
        char* end = nullptr;
        long x    = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str())
            return fallback;
        if (x < 0)
            return 0;
        if (x > 1000000)
            return 1000000;
        return static_cast<int>(x);
    }

    bool apply_forced_probe_from_env(ViewerState& viewer)
    {
        const int forced_x = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_X",
                                           -1);
        const int forced_y = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_Y",
                                           -1);
        if (forced_x < 0 || forced_y < 0 || viewer.image.path.empty())
            return false;

        const int px = std::clamp(forced_x, 0,
                                  std::max(0, viewer.image.width - 1));
        const int py = std::clamp(forced_y, 0,
                                  std::max(0, viewer.image.height - 1));
        std::vector<double> sampled;
        if (!sample_loaded_pixel(viewer.image, px, py, sampled))
            return false;

        viewer.probe_valid    = true;
        viewer.probe_x        = px;
        viewer.probe_y        = py;
        viewer.probe_channels = std::move(sampled);
        return true;
    }

    void current_child_visible_rect(const ImVec2& padding, bool scroll_x,
                                    bool scroll_y, ImVec2& out_min,
                                    ImVec2& out_max)
    {
        const ImVec2 window_pos  = ImGui::GetWindowPos();
        const ImVec2 window_size = ImGui::GetWindowSize();
        const ImGuiStyle& style  = ImGui::GetStyle();
        out_min = ImVec2(window_pos.x + padding.x, window_pos.y + padding.y);
        out_max = ImVec2(window_pos.x + window_size.x - padding.x,
                         window_pos.y + window_size.y - padding.y);
        if (scroll_y)
            out_max.x -= style.ScrollbarSize;
        if (scroll_x)
            out_max.y -= style.ScrollbarSize;
        out_max.x = std::max(out_min.x, out_max.x);
        out_max.y = std::max(out_min.y, out_max.y);
    }

}  // namespace

void
draw_image_window_contents(ViewerState& viewer, PlaceholderUiState& ui_state,
                           const AppFonts& fonts,
                           const PendingZoomRequest& shortcut_zoom_request,
                           bool recenter_requested)
{
    const float status_bar_height
        = std::max(30.0f, ImGui::GetTextLineHeightWithSpacing()
                              + ImGui::GetStyle().FramePadding.y * 2.0f + 8.0f);
    ImVec2 content_avail   = ImGui::GetContentRegionAvail();
    const float viewport_h = std::max(64.0f,
                                      content_avail.y - status_bar_height);

    const ImVec2 viewport_padding(8.0f, 8.0f);
    ImageViewportLayout image_layout;
    int display_width  = 0;
    int display_height = 0;
    if (!viewer.image.path.empty()) {
        display_width  = viewer.image.width;
        display_height = viewer.image.height;
        oriented_image_dimensions(viewer.image, display_width, display_height);
        if ((viewer.fit_request || ui_state.fit_image_to_window)
            && display_width > 0 && display_height > 0) {
            const ImVec2 child_size(content_avail.x, viewport_h);
            viewer.zoom = compute_fit_zoom(child_size, viewport_padding,
                                           display_width, display_height);
            viewer.zoom_pivot_pending     = false;
            viewer.zoom_pivot_frames_left = 0;
            viewer.norm_scroll            = ImVec2(0.5f, 0.5f);
            viewer.fit_request            = false;
            viewer.scroll_sync_frames_left
                = std::max(viewer.scroll_sync_frames_left, 2);
        }

        const ImVec2 image_size(static_cast<float>(display_width) * viewer.zoom,
                                static_cast<float>(display_height)
                                    * viewer.zoom);
        if (recenter_requested)
            recenter_view(viewer, image_size);
        image_layout
            = compute_image_viewport_layout(ImVec2(content_avail.x, viewport_h),
                                            viewport_padding, image_size,
                                            ImGui::GetStyle().ScrollbarSize);
        sync_view_scroll_from_display_scroll(
            viewer,
            ImVec2(viewer.norm_scroll.x * image_size.x,
                   viewer.norm_scroll.y * image_size.y),
            image_size);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, viewport_padding);
    if (!viewer.image.path.empty()
        && (image_layout.scroll_x || image_layout.scroll_y)) {
        ImGui::SetNextWindowContentSize(image_layout.content_size);
        if (viewer.scroll_sync_frames_left > 0)
            ImGui::SetNextWindowScroll(viewer.scroll);
    }
    ImGui::BeginChild("Viewport", ImVec2(0.0f, viewport_h), false,
                      ImGuiWindowFlags_HorizontalScrollbar
                          | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (!viewer.last_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
        ImGui::TextWrapped("%s", viewer.last_error.c_str());
        ImGui::PopStyleColor();
        register_layout_dump_synthetic_item("text", viewer.last_error.c_str());
    }

    const bool has_image = !viewer.image.path.empty();
    if (has_image) {
        if (ui_state.show_area_probe_window && viewer.area_probe_lines.empty())
            reset_area_probe_overlay(viewer);
        if (!ui_state.show_area_probe_window)
            viewer.area_probe_drag_active = false;

        const ImVec2 image_size = image_layout.image_size;
        ImTextureRef main_texture_ref;
        ImTextureRef closeup_texture_ref;
        bool has_main_texture           = false;
        bool has_closeup_texture        = false;
        bool image_canvas_pressed       = false;
        bool image_canvas_hovered       = false;
        bool image_canvas_active        = false;
        PendingZoomRequest pending_zoom = shortcut_zoom_request;

#if defined(IMIV_BACKEND_VULKAN_GLFW)
        const bool texture_ready_for_display
            = viewer.texture.preview_initialized;
        VkDescriptorSet main_set = ui_state.linear_interpolation
                                       ? viewer.texture.set
                                       : viewer.texture.nearest_mag_set;
        if (main_set == VK_NULL_HANDLE)
            main_set = viewer.texture.set;
        if (texture_ready_for_display && main_set != VK_NULL_HANDLE) {
            main_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                reinterpret_cast<uintptr_t>(main_set)));
            has_main_texture = true;
        }
        if (texture_ready_for_display
            && viewer.texture.pixelview_set != VK_NULL_HANDLE) {
            closeup_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                reinterpret_cast<uintptr_t>(viewer.texture.pixelview_set)));
            has_closeup_texture = true;
        } else if (texture_ready_for_display
                   && viewer.texture.set != VK_NULL_HANDLE) {
            closeup_texture_ref = main_texture_ref;
            has_closeup_texture = true;
        }
#endif
        ImageCoordinateMap coord_map;
        coord_map.source_width  = viewer.image.width;
        coord_map.source_height = viewer.image.height;
        coord_map.orientation   = viewer.image.orientation;
        current_child_visible_rect(viewport_padding, image_layout.scroll_x,
                                   image_layout.scroll_y,
                                   coord_map.viewport_rect_min,
                                   coord_map.viewport_rect_max);
        const ImVec2 viewport_center = rect_center(coord_map.viewport_rect_min,
                                                   coord_map.viewport_rect_max);
        const bool can_scroll_x      = image_layout.scroll_x;
        const bool can_scroll_y      = image_layout.scroll_y;
        if (viewer.zoom_pivot_pending || viewer.zoom_pivot_frames_left > 0) {
            apply_pending_zoom_pivot(viewer, coord_map, image_size,
                                     can_scroll_x, can_scroll_y);
        } else if (viewer.scroll_sync_frames_left > 0) {
            if (can_scroll_x)
                ImGui::SetScrollX(viewer.scroll.x);
            else
                ImGui::SetScrollX(0.0f);
            if (can_scroll_y)
                ImGui::SetScrollY(viewer.scroll.y);
            else
                ImGui::SetScrollY(0.0f);
            --viewer.scroll_sync_frames_left;
        } else {
            ImVec2 imgui_scroll = viewer.scroll;
            if (can_scroll_x)
                imgui_scroll.x = ImGui::GetScrollX();
            if (can_scroll_y)
                imgui_scroll.y = ImGui::GetScrollY();
            sync_view_scroll_from_display_scroll(viewer, imgui_scroll,
                                                 image_size);
        }
        coord_map.valid          = (image_size.x > 0.0f && image_size.y > 0.0f);
        coord_map.image_rect_min = ImVec2(viewport_center.x - viewer.scroll.x,
                                          viewport_center.y - viewer.scroll.y);
        coord_map.image_rect_max
            = ImVec2(coord_map.image_rect_min.x + image_size.x,
                     coord_map.image_rect_min.y + image_size.y);
        update_test_engine_mouse_space(
            coord_map.viewport_rect_min, coord_map.viewport_rect_max,
            coord_map.valid ? coord_map.image_rect_min : ImVec2(0.0f, 0.0f),
            coord_map.valid ? coord_map.image_rect_max : ImVec2(0.0f, 0.0f));
        coord_map.window_pos = ImGui::GetWindowPos();
        if (has_main_texture && coord_map.valid) {
            ImGui::SetCursorScreenPos(coord_map.image_rect_min);
            image_canvas_pressed = ImGui::InvisibleButton(
                "##image_canvas", image_size,
                ImGuiButtonFlags_MouseButtonLeft
                    | ImGuiButtonFlags_MouseButtonRight
                    | ImGuiButtonFlags_MouseButtonMiddle);
            image_canvas_hovered = ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            image_canvas_active = ImGui::IsItemActive();
            register_layout_dump_synthetic_item("image", "Image");
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->PushClipRect(coord_map.viewport_rect_min,
                                    coord_map.viewport_rect_max, true);
            draw_list->AddImage(main_texture_ref, coord_map.image_rect_min,
                                coord_map.image_rect_max);
            draw_list->PopClipRect();
        } else if (!has_main_texture) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            const bool texture_loading
                = viewer.texture.upload_submit_pending
                  || viewer.texture.preview_submit_pending
                  || (!viewer.texture.preview_initialized
                      && viewer.texture.set != VK_NULL_HANDLE);
            ImGui::TextUnformatted(texture_loading ? "Loading texture"
                                                   : "No texture");
            register_layout_dump_synthetic_item("text", texture_loading
                                                            ? "Loading texture"
                                                            : "No texture");
#else
            ImGui::TextUnformatted("No texture");
            register_layout_dump_synthetic_item("text", "No texture");
#endif
        }

        if (ui_state.show_window_guides && coord_map.valid) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(coord_map.image_rect_min,
                               coord_map.image_rect_max,
                               IM_COL32(250, 210, 80, 255), 0.0f, 0, 1.5f);
            draw_list->AddRect(coord_map.viewport_rect_min,
                               coord_map.viewport_rect_max,
                               IM_COL32(80, 200, 255, 220), 0.0f, 0, 1.0f);

            ImVec2 center_screen(0.0f, 0.0f);
            if (source_uv_to_screen(coord_map, ImVec2(0.5f, 0.5f),
                                    center_screen)) {
                const float r = 6.0f;
                draw_list->AddLine(ImVec2(center_screen.x - r, center_screen.y),
                                   ImVec2(center_screen.x + r, center_screen.y),
                                   IM_COL32(255, 170, 60, 255), 1.3f);
                draw_list->AddLine(ImVec2(center_screen.x, center_screen.y - r),
                                   ImVec2(center_screen.x, center_screen.y + r),
                                   IM_COL32(255, 170, 60, 255), 1.3f);
            }
        }

        const ImGuiIO& io          = ImGui::GetIO();
        const ImVec2 mouse         = io.MousePos;
        const bool area_probe_mode = ui_state.show_area_probe_window;
        const bool mouse_in_image  = point_in_rect(mouse,
                                                   coord_map.image_rect_min,
                                                   coord_map.image_rect_max);
        const bool mouse_in_viewport
            = point_in_rect(mouse, coord_map.viewport_rect_min,
                            coord_map.viewport_rect_max);
        const bool viewport_hovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_None);
        const bool viewport_accepts_mouse = viewport_hovered
                                            && mouse_in_viewport;
        const bool image_canvas_accepts_mouse = image_canvas_hovered
                                                || image_canvas_active;
        const bool image_canvas_clicked_left
            = image_canvas_pressed && io.MouseReleased[ImGuiMouseButton_Left];
        const bool image_canvas_clicked_right
            = image_canvas_pressed && io.MouseReleased[ImGuiMouseButton_Right];
        const bool empty_viewport_clicked_left
            = viewport_accepts_mouse && !mouse_in_image
              && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool empty_viewport_clicked_right
            = viewport_accepts_mouse && !mouse_in_image
              && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        const ImVec2 clamped_mouse
            = clamp_pos_to_rect(mouse, coord_map.image_rect_min,
                                coord_map.image_rect_max);

        ImVec2 source_uv(0.0f, 0.0f);
        int px = 0;
        int py = 0;
        std::vector<double> sampled;
        if (viewport_accepts_mouse && mouse_in_image
            && screen_to_source_uv(coord_map, mouse, source_uv)
            && source_uv_to_pixel(coord_map, source_uv, px, py)
            && sample_loaded_pixel(viewer.image, px, py, sampled)) {
            viewer.probe_valid    = true;
            viewer.probe_x        = px;
            viewer.probe_y        = py;
            viewer.probe_channels = std::move(sampled);
        } else if (ui_state.pixelview_follows_mouse
                   && (!viewport_accepts_mouse || !mouse_in_image)) {
            viewer.probe_valid = false;
            viewer.probe_channels.clear();
        }

        if (area_probe_mode) {
            ImVec2 area_source_uv(0.5f, 0.5f);
            const bool have_area_source_uv
                = screen_to_source_uv(coord_map, clamped_mouse, area_source_uv);
            const bool area_probe_accepts_mouse
                = viewport_accepts_mouse || image_canvas_accepts_mouse
                  || viewer.area_probe_drag_active;
            if (!viewer.area_probe_drag_active && area_probe_accepts_mouse
                && ImGui::IsMouseDown(ImGuiMouseButton_Left)
                && have_area_source_uv) {
                viewer.area_probe_drag_active   = true;
                viewer.area_probe_drag_start_uv = area_source_uv;
                viewer.area_probe_drag_end_uv   = area_source_uv;
            }
            if (viewer.area_probe_drag_active && have_area_source_uv)
                viewer.area_probe_drag_end_uv = area_source_uv;
            if (viewer.area_probe_drag_active) {
                int xbegin = 0;
                int ybegin = 0;
                int xend   = 0;
                int yend   = 0;
                if (source_uv_to_pixel(coord_map,
                                       viewer.area_probe_drag_start_uv, xbegin,
                                       ybegin)
                    && source_uv_to_pixel(coord_map,
                                          viewer.area_probe_drag_end_uv, xend,
                                          yend)) {
                    update_area_probe_overlay(viewer, xbegin, ybegin, xend,
                                              yend);
                }
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    viewer.area_probe_drag_active = false;
            }
        }

        bool want_pan       = false;
        bool want_zoom_drag = false;
        if (viewport_accepts_mouse || image_canvas_accepts_mouse
            || viewer.pan_drag_active || viewer.zoom_drag_active) {
            if (ui_state.mouse_mode == 1) {
                want_pan = (!area_probe_mode
                            && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                           || ImGui::IsMouseDown(ImGuiMouseButton_Right)
                           || ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            } else if (ui_state.mouse_mode == 0) {
                const bool want_middle_pan
                    = ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                      && (viewer.pan_drag_active || image_canvas_accepts_mouse
                          || viewport_accepts_mouse);
                const bool want_alt_left_pan
                    = !area_probe_mode && io.KeyAlt
                      && ImGui::IsMouseDown(ImGuiMouseButton_Left)
                      && (viewer.pan_drag_active || image_canvas_accepts_mouse
                          || viewport_accepts_mouse);
                want_pan       = want_middle_pan || want_alt_left_pan;
                want_zoom_drag = io.KeyAlt
                                 && ImGui::IsMouseDown(ImGuiMouseButton_Right)
                                 && (viewer.zoom_drag_active
                                     || image_canvas_accepts_mouse
                                     || viewport_accepts_mouse);
                if (!area_probe_mode && !io.KeyAlt
                    && (image_canvas_clicked_left || image_canvas_clicked_right
                        || empty_viewport_clicked_left
                        || empty_viewport_clicked_right)) {
                    if (image_canvas_clicked_left
                        || empty_viewport_clicked_left) {
                        request_zoom_scale(pending_zoom, 2.0f, true);
                    }
                    if (image_canvas_clicked_right
                        || empty_viewport_clicked_right) {
                        request_zoom_scale(pending_zoom, 0.5f, true);
                    }
                }
            }
        }

        if (want_pan) {
            if (!viewer.pan_drag_active) {
                viewer.pan_drag_active = true;
                viewer.drag_prev_mouse = mouse;
            } else {
                const float dx = mouse.x - viewer.drag_prev_mouse.x;
                const float dy = mouse.y - viewer.drag_prev_mouse.y;
                sync_view_scroll_from_display_scroll(
                    viewer, ImVec2(viewer.scroll.x - dx, viewer.scroll.y - dy),
                    image_size);
                viewer.scroll_sync_frames_left
                    = std::max(viewer.scroll_sync_frames_left, 2);
                viewer.drag_prev_mouse       = mouse;
                viewer.fit_request           = false;
                ui_state.fit_image_to_window = false;
            }
        } else {
            viewer.pan_drag_active = false;
        }

        if (want_zoom_drag) {
            if (!viewer.zoom_drag_active) {
                viewer.zoom_drag_active = true;
                viewer.drag_prev_mouse  = mouse;
            } else {
                const float dx    = mouse.x - viewer.drag_prev_mouse.x;
                const float dy    = mouse.y - viewer.drag_prev_mouse.y;
                const float scale = 1.0f + 0.005f * (dx + dy);
                if (scale > 0.0f)
                    request_zoom_scale(pending_zoom, scale, true);
                viewer.drag_prev_mouse = mouse;
            }
        } else {
            viewer.zoom_drag_active = false;
        }

        if (viewport_accepts_mouse && io.MouseWheel != 0.0f) {
            const float scale = (io.MouseWheel > 0.0f) ? 1.1f : 0.9f;
            request_zoom_scale(pending_zoom, scale, true);
        }

        apply_zoom_request(coord_map, viewer, ui_state, pending_zoom, mouse);

        if (apply_forced_probe_from_env(viewer))
            viewer.probe_valid = true;

        const OverlayPanelRect pixel_panel
            = draw_pixel_closeup_overlay(viewer, ui_state, coord_map,
                                         closeup_texture_ref,
                                         has_closeup_texture, fonts);
        draw_area_probe_overlay(viewer, ui_state, coord_map, pixel_panel,
                                fonts);
    } else if (viewer.last_error.empty()) {
        viewer.probe_valid = false;
        viewer.probe_channels.clear();
        draw_padded_message("No image loaded. Use File/Open to load an image.");
        register_layout_dump_synthetic_item("text", "No image loaded.");
    }

    ImGui::EndChild();
    ImGui::Separator();
    register_layout_dump_synthetic_item("divider", "Main viewport");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::BeginChild("StatusBarRegion", ImVec2(0.0f, status_bar_height), false,
                      ImGuiWindowFlags_NoScrollbar
                          | ImGuiWindowFlags_NoScrollWithMouse);
    draw_embedded_status_bar(viewer, ui_state);
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

}  // namespace Imiv
