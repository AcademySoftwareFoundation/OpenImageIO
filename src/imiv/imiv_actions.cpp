// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_actions.h"

#include "imiv_file_dialog.h"
#include "imiv_image_library.h"
#include "imiv_loaded_image.h"
#include "imiv_ocio.h"
#include "imiv_parse.h"
#include "imiv_probe_overlay.h"
#include "imiv_ui.h"
#include "imiv_viewer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
#    define GLFW_INCLUDE_NONE
#    include <GLFW/glfw3.h>
#endif

#include <imgui.h>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    void clear_loaded_image_state(ViewerState& viewer)
    {
        viewer.image       = LoadedImage();
        viewer.zoom        = 1.0f;
        viewer.fit_request = true;
        reset_view_navigation_state(viewer);
        viewer.probe_valid = false;
        viewer.probe_channels.clear();
        reset_area_probe_overlay(viewer);
    }

    void calc_subimage_from_zoom(const LoadedImage& image, int& subimage,
                                 float& zoom)
    {
        const int rel_subimage = static_cast<int>(
            std::trunc(std::log2(std::max(1.0e-6f, 1.0f / zoom))));
        subimage = std::clamp(image.subimage + rel_subimage, 0,
                              image.nsubimages - 1);
        if (!(image.subimage == 0 && zoom > 1.0f)
            && !(image.subimage == image.nsubimages - 1 && zoom < 1.0f)) {
            const float pow_zoom = std::pow(2.0f,
                                            static_cast<float>(rel_subimage));
            zoom *= pow_zoom;
        }
    }

    void restore_view_after_subimage_load(ViewerState& viewer, float zoom,
                                          const ImVec2& norm_scroll)
    {
        int display_width  = viewer.image.width;
        int display_height = viewer.image.height;
        oriented_image_dimensions(viewer.image, display_width, display_height);
        viewer.zoom        = zoom;
        viewer.fit_request = false;
        const ImVec2 image_size(static_cast<float>(display_width) * viewer.zoom,
                                static_cast<float>(display_height)
                                    * viewer.zoom);
        sync_view_scroll_from_display_scroll(
            viewer,
            ImVec2(std::clamp(norm_scroll.x, 0.0f, 1.0f) * image_size.x,
                   std::clamp(norm_scroll.y, 0.0f, 1.0f) * image_size.y),
            image_size);
        viewer.scroll_sync_frames_left
            = std::max(viewer.scroll_sync_frames_left, 2);
    }

}  // namespace

bool
viewer_texture_has_gpu_lifetime(const RendererTexture& texture)
{
    return texture.backend != nullptr;
}

void
quiesce_viewer_texture_lifetime(RendererState& renderer_state,
                                const RendererTexture& texture)
{
    if (!viewer_texture_has_gpu_lifetime(texture))
        return;
    std::string error_message;
    renderer_wait_idle(renderer_state, error_message);
}

bool
load_viewer_image(RendererState& vk_state, ViewerState& viewer,
                  ImageLibraryState& library, PlaceholderUiState* ui_state,
                  const std::string& path, int requested_subimage,
                  int requested_miplevel)
{
    viewer.last_error.clear();
    const std::string previous_path = viewer.image.path;
    const int previous_index        = viewer.current_path_index;
    LoadedImage loaded;
    std::string error;
    if (!load_image_for_compute(path, requested_subimage, requested_miplevel,
                                viewer.rawcolor, loaded, error)) {
        viewer.last_error = Strutil::fmt::format("open failed: {}", error);
        print(stderr, "imiv: {}\n", viewer.last_error);
        return false;
    }
    RendererTexture texture;
    if (!renderer_create_texture(vk_state, loaded, texture, error)) {
        viewer.last_error = Strutil::fmt::format("upload failed: {}", error);
        print(stderr, "imiv: {}\n", viewer.last_error);
        return false;
    }
    quiesce_viewer_texture_lifetime(vk_state, viewer.texture);
    renderer_destroy_texture(vk_state, viewer.texture);
    if (should_reset_preview_on_load(viewer, path))
        reset_per_image_preview_state(viewer.recipe);
    viewer.image       = std::move(loaded);
    viewer.texture     = std::move(texture);
    viewer.zoom        = 1.0f;
    viewer.fit_request = true;
    reset_view_navigation_state(viewer);
    viewer.probe_valid = false;
    viewer.probe_channels.clear();
    reset_area_probe_overlay(viewer);
    if (viewer.image.width > 0 && viewer.image.height > 0) {
        const int center_x = viewer.image.width / 2;
        const int center_y = viewer.image.height / 2;
        std::vector<double> sample;
        if (sample_loaded_pixel(viewer.image, center_x, center_y, sample)) {
            viewer.probe_valid    = true;
            viewer.probe_x        = center_x;
            viewer.probe_y        = center_y;
            viewer.probe_channels = std::move(sample);
        }
    }
    int loaded_index = -1;
    add_loaded_image_path(library, viewer.image.path, &loaded_index);
    viewer.loaded_image_paths = library.loaded_image_paths;
    viewer.recent_images      = library.recent_images;
    viewer.sort_mode          = library.sort_mode;
    viewer.sort_reverse       = library.sort_reverse;
    if (!previous_path.empty() && previous_index >= 0
        && previous_path != viewer.image.path
        && previous_index != loaded_index) {
        viewer.last_path_index = previous_index;
    }
    viewer.current_path_index = loaded_index;
    add_recent_image_path(library, viewer.image.path);
    viewer.recent_images  = library.recent_images;
    viewer.status_message = Strutil::fmt::format(
        "Loaded {} ({}x{}, {} channels, {}, subimage {}/{}, mip {}/{})",
        viewer.image.path, viewer.image.width, viewer.image.height,
        viewer.image.nchannels, upload_data_type_name(viewer.image.type),
        viewer.image.subimage + 1, viewer.image.nsubimages,
        viewer.image.miplevel + 1, viewer.image.nmiplevels);
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
        || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
        || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
        || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML")) {
        print("imiv: {}\n", viewer.status_message);
    }
    return true;
}

void
set_placeholder_status(ViewerState& viewer, const char* action)
{
    viewer.status_message = Strutil::fmt::format("{} (not implemented yet)",
                                                 action);
    viewer.last_error.clear();
}

void
set_full_screen_mode(GLFWwindow* window, ViewerState& viewer, bool enable,
                     std::string& error_message)
{
    error_message.clear();
    if (window == nullptr)
        return;
    if (enable == viewer.fullscreen_applied)
        return;

    if (enable) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr) {
            error_message = "fullscreen failed: no primary monitor";
            return;
        }
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr) {
            error_message = "fullscreen failed: monitor mode unavailable";
            return;
        }

        glfwGetWindowPos(window, &viewer.windowed_x, &viewer.windowed_y);
        glfwGetWindowSize(window, &viewer.windowed_width,
                          &viewer.windowed_height);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                             mode->refreshRate);
        viewer.fullscreen_applied = true;
        return;
    }

    const int restore_w = std::max(320, viewer.windowed_width);
    const int restore_h = std::max(240, viewer.windowed_height);
    glfwSetWindowMonitor(window, nullptr, viewer.windowed_x, viewer.windowed_y,
                         restore_w, restore_h, 0);
    viewer.fullscreen_applied = false;
}

void
fit_window_to_image_action(GLFWwindow* window, ViewerState& viewer,
                           PlaceholderUiState& ui_state)
{
    if (window == nullptr || viewer.image.path.empty())
        return;
    if (viewer.fullscreen_applied || ui_state.full_screen_mode)
        return;

    int window_w = 0;
    int window_h = 0;
    int fb_w     = 0;
    int fb_h     = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwGetFramebufferSize(window, &fb_w, &fb_h);

    const float scale_x = (fb_w > 0) ? (static_cast<float>(window_w) / fb_w)
                                     : 1.0f;
    const float scale_y = (fb_h > 0) ? (static_cast<float>(window_h) / fb_h)
                                     : 1.0f;

    constexpr int k_view_padding_px = 24;
    constexpr int k_ui_overhead_px  = 120;
    int display_width               = viewer.image.width;
    int display_height              = viewer.image.height;
    oriented_image_dimensions(viewer.image, display_width, display_height);
    const int target_fb_w = std::max(320, display_width + k_view_padding_px);
    const int target_fb_h = std::max(240, display_height + k_ui_overhead_px);
    const int target_w    = static_cast<int>(
        std::round(static_cast<float>(target_fb_w) * scale_x));
    const int target_h = static_cast<int>(
        std::round(static_cast<float>(target_fb_h) * scale_y));

    glfwSetWindowSize(window, target_w, target_h);
    ui_state.fit_image_to_window = false;
    viewer.zoom                  = 1.0f;
    viewer.fit_request           = false;
    viewer.status_message = Strutil::fmt::format("Fit window to image: {}x{}",
                                                 target_w, target_h);
    viewer.last_error.clear();
}

void
select_all_image_action(ViewerState& viewer, const PlaceholderUiState& ui_state)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    set_image_selection(viewer, 0, 0, viewer.image.width, viewer.image.height);
    sync_area_probe_to_selection(viewer, ui_state);
    viewer.status_message = Strutil::fmt::format("Selected full image ({}x{})",
                                                 viewer.image.width,
                                                 viewer.image.height);
    viewer.last_error.clear();
}

void
deselect_selection_action(ViewerState& viewer,
                          const PlaceholderUiState& ui_state)
{
    if (!has_image_selection(viewer)) {
        sync_area_probe_to_selection(viewer, ui_state);
        viewer.status_message = "No selection";
        viewer.last_error.clear();
        return;
    }
    clear_image_selection(viewer);
    sync_area_probe_to_selection(viewer, ui_state);
    viewer.status_message = "Selection cleared";
    viewer.last_error.clear();
}

void
set_area_sample_enabled(ViewerState& viewer, PlaceholderUiState& ui_state,
                        bool enabled)
{
    ui_state.show_area_probe_window = enabled;
    if (enabled) {
        ui_state.mouse_mode = 3;
        sync_area_probe_to_selection(viewer, ui_state);
        return;
    }

    if (ui_state.mouse_mode == 3)
        ui_state.mouse_mode = 0;
    clear_image_selection(viewer);
    reset_area_probe_overlay(viewer);
}

void
set_mouse_mode_action(ViewerState& viewer, PlaceholderUiState& ui_state,
                      int mouse_mode)
{
    ui_state.mouse_mode = std::clamp(mouse_mode, 0, 4);
    if (ui_state.mouse_mode == 3)
        ui_state.mouse_mode = 0;
    if (ui_state.show_area_probe_window) {
        set_area_sample_enabled(viewer, ui_state, false);
    }
}

void
set_sort_mode_action(ImageLibraryState& library,
                     const std::vector<ViewerState*>& viewers,
                     ImageSortMode mode)
{
    library.sort_mode = mode;
    sort_loaded_image_paths(library, viewers);
    for (ViewerState* viewer : viewers) {
        if (viewer == nullptr)
            continue;
        sync_viewer_library_state(*viewer, library);
        viewer->status_message = "Image list sort mode changed";
        viewer->last_error.clear();
    }
}

void
toggle_sort_reverse_action(ImageLibraryState& library,
                           const std::vector<ViewerState*>& viewers)
{
    library.sort_reverse = !library.sort_reverse;
    sort_loaded_image_paths(library, viewers);
    for (ViewerState* viewer : viewers) {
        if (viewer == nullptr)
            continue;
        sync_viewer_library_state(*viewer, library);
        viewer->status_message = library.sort_reverse
                                     ? "Image list order reversed"
                                     : "Image list order restored";
        viewer->last_error.clear();
    }
}

bool
advance_slide_show_action(RendererState& vk_state, ViewerState& viewer,
                          ImageLibraryState& library,
                          PlaceholderUiState& ui_state)
{
    if (!ui_state.slide_show_running || library.loaded_image_paths.empty()
        || viewer.image.path.empty()) {
        return false;
    }

    const int count = static_cast<int>(library.loaded_image_paths.size());
    if (count <= 0 || viewer.current_path_index < 0)
        return false;

    if (!ui_state.slide_loop && viewer.current_path_index >= count - 1) {
        ui_state.slide_show_running = false;
        viewer.status_message       = "Slide show reached final image";
        viewer.last_error.clear();
        return false;
    }

    std::string next_path;
    if (!pick_loaded_image_path(library, viewer, 1, next_path)
        || next_path.empty())
        return false;
    return load_viewer_image(vk_state, viewer, library, &ui_state, next_path,
                             viewer.image.subimage, viewer.image.miplevel);
}

void
toggle_slide_show_action(PlaceholderUiState& ui_state, ViewerState& viewer)
{
    ui_state.slide_show_running = !ui_state.slide_show_running;
    if (ui_state.slide_show_running)
        ui_state.full_screen_mode = true;
    viewer.slide_last_advance_time = ImGui::GetTime();
    viewer.status_message = ui_state.slide_show_running ? "Slide show started"
                                                        : "Slide show stopped";
    viewer.last_error.clear();
}

void
reload_current_image_action(RendererState& vk_state, ViewerState& viewer,
                            ImageLibraryState& library,
                            PlaceholderUiState& ui_state)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    load_viewer_image(vk_state, viewer, library, &ui_state, viewer.image.path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
close_current_image_action(RendererState& vk_state, ViewerState& viewer,
                           ImageLibraryState& library,
                           PlaceholderUiState& ui_state)
{
    quiesce_viewer_texture_lifetime(vk_state, viewer.texture);
    renderer_destroy_texture(vk_state, viewer.texture);
    clear_loaded_image_state(viewer);
    viewer.loaded_image_paths = library.loaded_image_paths;
    viewer.recent_images      = library.recent_images;
    viewer.sort_mode          = library.sort_mode;
    viewer.sort_reverse       = library.sort_reverse;
    viewer.current_path_index = -1;
    viewer.last_path_index    = -1;
    viewer.last_error.clear();
    viewer.status_message = "Closed current image";
}

void
next_sibling_image_action(RendererState& vk_state, ViewerState& viewer,
                          ImageLibraryState& library,
                          PlaceholderUiState& ui_state, int delta)
{
    std::string path;
    if (!pick_loaded_image_path(library, viewer, delta, path)) {
        viewer.status_message = (delta < 0) ? "Previous image unavailable"
                                            : "Next image unavailable";
        viewer.last_error.clear();
        return;
    }
    load_viewer_image(vk_state, viewer, library, &ui_state, path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
toggle_image_action(RendererState& vk_state, ViewerState& viewer,
                    ImageLibraryState& library, PlaceholderUiState& ui_state)
{
    if (viewer.last_path_index < 0
        || viewer.last_path_index
               >= static_cast<int>(library.loaded_image_paths.size())) {
        viewer.status_message = "No toggled image available";
        viewer.last_error.clear();
        return;
    }
    const std::string toggle_path
        = library
              .loaded_image_paths[static_cast<size_t>(viewer.last_path_index)];
    load_viewer_image(vk_state, viewer, library, &ui_state, toggle_path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
change_subimage_action(RendererState& vk_state, ViewerState& viewer,
                       ImageLibraryState& library, PlaceholderUiState& ui_state,
                       int delta)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    viewer.pending_auto_subimage = -1;
    bool ok                      = false;
    if (delta < 0) {
        if (viewer.image.miplevel > 0) {
            viewer.auto_subimage = false;
            ok = load_viewer_image(vk_state, viewer, library, &ui_state,
                                   viewer.image.path, viewer.image.subimage,
                                   viewer.image.miplevel - 1);
        } else if (viewer.image.subimage > 0) {
            viewer.auto_subimage = false;
            ok = load_viewer_image(vk_state, viewer, library, &ui_state,
                                   viewer.image.path, viewer.image.subimage - 1,
                                   0);
        } else if (viewer.image.nsubimages > 1) {
            viewer.auto_subimage  = true;
            viewer.status_message = "Auto subimage enabled";
            viewer.last_error.clear();
        }
    } else if (delta > 0) {
        if (viewer.auto_subimage) {
            viewer.auto_subimage = false;
            ok = load_viewer_image(vk_state, viewer, library, &ui_state,
                                   viewer.image.path, 0, 0);
        } else if (viewer.image.miplevel < viewer.image.nmiplevels - 1) {
            ok = load_viewer_image(vk_state, viewer, library, &ui_state,
                                   viewer.image.path, viewer.image.subimage,
                                   viewer.image.miplevel + 1);
        } else if (viewer.image.subimage < viewer.image.nsubimages - 1) {
            ok = load_viewer_image(vk_state, viewer, library, &ui_state,
                                   viewer.image.path, viewer.image.subimage + 1,
                                   0);
        }
    }
    if (ok)
        viewer.last_error.clear();
}

void
change_miplevel_action(RendererState& vk_state, ViewerState& viewer,
                       ImageLibraryState& library, PlaceholderUiState& ui_state,
                       int delta)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    const int target_mip = viewer.image.miplevel + delta;
    if (target_mip < 0 || target_mip >= viewer.image.nmiplevels)
        return;
    viewer.auto_subimage         = false;
    viewer.pending_auto_subimage = -1;
    load_viewer_image(vk_state, viewer, library, &ui_state, viewer.image.path,
                      viewer.image.subimage, target_mip);
}

void
queue_auto_subimage_from_zoom(ViewerState& viewer)
{
    viewer.pending_auto_subimage = -1;
    if (!viewer.auto_subimage || viewer.image.path.empty()
        || viewer.image.nsubimages <= 1) {
        return;
    }
    int target_subimage = viewer.image.subimage;
    float adjusted_zoom = viewer.zoom;
    calc_subimage_from_zoom(viewer.image, target_subimage, adjusted_zoom);
    if (target_subimage == viewer.image.subimage)
        return;
    viewer.pending_auto_subimage             = target_subimage;
    viewer.pending_auto_subimage_zoom        = adjusted_zoom;
    viewer.pending_auto_subimage_norm_scroll = viewer.norm_scroll;
}

bool
apply_pending_auto_subimage_action(RendererState& vk_state, ViewerState& viewer,
                                   ImageLibraryState& library,
                                   PlaceholderUiState& ui_state)
{
    if (viewer.pending_auto_subimage < 0 || viewer.image.path.empty())
        return false;
    const int target_subimage     = viewer.pending_auto_subimage;
    const float adjusted_zoom     = viewer.pending_auto_subimage_zoom;
    const ImVec2 preserved_scroll = viewer.pending_auto_subimage_norm_scroll;
    const bool auto_mode          = viewer.auto_subimage;
    viewer.pending_auto_subimage  = -1;
    if (target_subimage < 0 || target_subimage >= viewer.image.nsubimages)
        return false;
    if (!load_viewer_image(vk_state, viewer, library, &ui_state,
                           viewer.image.path, target_subimage, 0)) {
        return false;
    }
    viewer.auto_subimage = auto_mode;
    restore_view_after_subimage_load(viewer, adjusted_zoom, preserved_scroll);
    return true;
}

}  // namespace Imiv
