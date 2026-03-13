// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Imiv {

enum class ImageSortMode : uint8_t {
    ByName      = 0,
    ByPath      = 1,
    ByImageDate = 2,
    ByFileDate  = 3
};

enum class OcioConfigSource : uint8_t { Global = 0, BuiltIn = 1, User = 2 };

struct ViewerState {
    LoadedImage image;
    std::string status_message;
    std::string last_error;
    bool rawcolor               = false;
    bool no_autopremult         = false;
    float zoom                  = 1.0f;
    bool fit_request            = true;
    ImVec2 scroll               = ImVec2(0.0f, 0.0f);
    ImVec2 norm_scroll          = ImVec2(0.5f, 0.5f);
    ImVec2 max_scroll           = ImVec2(0.0f, 0.0f);
    ImVec2 zoom_pivot_screen    = ImVec2(0.0f, 0.0f);
    ImVec2 zoom_pivot_source_uv = ImVec2(0.5f, 0.5f);
    bool zoom_pivot_pending     = false;
    int zoom_pivot_frames_left  = 0;
    int scroll_sync_frames_left = 0;
    std::vector<std::string> loaded_image_paths;
    int current_path_index = -1;
    int last_path_index    = -1;
    std::string toggle_image_path;
    std::vector<std::string> recent_images;
    ImageSortMode sort_mode = ImageSortMode::ByName;
    bool sort_reverse       = false;
    bool probe_valid        = false;
    int probe_x             = 0;
    int probe_y             = 0;
    std::vector<double> probe_channels;
    bool area_probe_drag_active     = false;
    ImVec2 area_probe_drag_start_uv = ImVec2(0.0f, 0.0f);
    ImVec2 area_probe_drag_end_uv   = ImVec2(0.0f, 0.0f);
    std::vector<std::string> area_probe_lines;
    bool pan_drag_active           = false;
    bool zoom_drag_active          = false;
    ImVec2 drag_prev_mouse         = ImVec2(0.0f, 0.0f);
    bool fullscreen_applied        = false;
    int windowed_x                 = 100;
    int windowed_y                 = 100;
    int windowed_width             = 1600;
    int windowed_height            = 900;
    double slide_last_advance_time = 0.0;
    bool drag_overlay_active       = false;
    std::vector<std::string> pending_drop_paths;
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    VulkanTexture texture;
#endif
};

struct PlaceholderUiState {
    bool show_info_window         = false;
    bool show_preferences_window  = false;
    bool show_preview_window      = false;
    bool show_pixelview_window    = false;
    bool show_area_probe_window   = false;
    bool show_window_guides       = false;
    bool show_mouse_mode_selector = false;
    bool fit_image_to_window      = false;
    bool full_screen_mode         = false;
    bool slide_show_running       = false;
    bool slide_loop               = true;
    bool use_ocio                 = false;
    bool pixelview_follows_mouse  = false;
    bool pixelview_left_corner    = true;
    bool linear_interpolation     = false;
    bool dark_palette             = true;
    bool auto_mipmap              = false;
    bool image_window_force_dock  = true;

    int max_memory_ic_mb       = 2048;
    int slide_duration_seconds = 10;
    int closeup_pixels         = 13;
    int closeup_avg_pixels     = 11;
    int current_channel        = 0;
    int subimage_index         = 0;
    int miplevel_index         = 0;
    int color_mode             = 0;
    int mouse_mode             = 0;

    float exposure = 0.0f;
    float gamma    = 1.0f;
    float offset   = 0.0f;

    int ocio_config_source   = static_cast<int>(OcioConfigSource::Global);
    std::string ocio_display = "default";
    std::string ocio_view    = "default";
    std::string ocio_image_color_space = "auto";
    std::string ocio_user_config_path;
};

void
reset_view_navigation_state(ViewerState& viewer);
void
clamp_placeholder_ui_state(PlaceholderUiState& ui_state);
void
reset_per_image_preview_state(PlaceholderUiState& ui_state);
bool
load_persistent_state(PlaceholderUiState& ui_state, ViewerState& viewer,
                      std::string& error_message);
bool
save_persistent_state(const PlaceholderUiState& ui_state,
                      const ViewerState& viewer, std::string& error_message);
int
clamp_orientation(int orientation);
void
oriented_image_dimensions(const LoadedImage& image, int& out_width,
                          int& out_height);
bool
load_image_for_compute(const std::string& path, int requested_subimage,
                       int requested_miplevel, bool rawcolor,
                       LoadedImage& image, std::string& error_message);
bool
should_reset_preview_on_load(const ViewerState& viewer,
                             const std::string& path);
bool
add_loaded_image_path(ViewerState& viewer, const std::string& path,
                      int* out_index = nullptr);
bool
append_loaded_image_paths(ViewerState& viewer,
                          const std::vector<std::string>& paths,
                          int* out_first_added_index = nullptr);
bool
remove_loaded_image_path(ViewerState& viewer, const std::string& path);
bool
set_current_loaded_image_path(ViewerState& viewer, const std::string& path);
bool
pick_loaded_image_path(const ViewerState& viewer, int delta,
                       std::string& out_path);
void
sort_loaded_image_paths(ViewerState& viewer);
void
add_recent_image_path(ViewerState& viewer, const std::string& path);

}  // namespace Imiv
