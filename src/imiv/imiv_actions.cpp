// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_actions.h"

#include "imiv_file_dialog.h"
#include "imiv_ui.h"

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

    std::filesystem::path executable_directory_path()
    {
        const std::string program_path = Sysutil::this_program_path();
        if (program_path.empty())
            return std::filesystem::path();
        return std::filesystem::path(program_path).parent_path();
    }

    std::filesystem::path default_screenshot_output_path()
    {
        std::filesystem::path base_dir = executable_directory_path();
        if (base_dir.empty())
            base_dir = std::filesystem::current_path();
        base_dir /= "screenshots";

        std::tm local_tm      = {};
        const std::time_t now = std::time(nullptr);
#if defined(_WIN32)
        localtime_s(&local_tm, &now);
#else
        localtime_r(&now, &local_tm);
#endif

        char timestamp[64] = {};
        if (std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S",
                          &local_tm)
            == 0) {
            std::snprintf(timestamp, sizeof(timestamp), "%lld",
                          static_cast<long long>(now));
        }

        std::filesystem::path path
            = base_dir / Strutil::fmt::format("imiv_{}.png", timestamp);
        for (int suffix = 1; std::filesystem::exists(path); ++suffix) {
            path = base_dir
                   / Strutil::fmt::format("imiv_{}_{:02d}.png", timestamp,
                                          suffix);
        }
        return path;
    }

}  // namespace

bool
read_env_value(const char* name, std::string& out_value)
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

bool
env_flag_is_truthy(const char* name)
{
    std::string value;
    if (!read_env_value(name, value))
        return false;

    const string_view trimmed = Strutil::strip(value);
    if (trimmed.empty())
        return false;
    if (trimmed == "1")
        return true;
    if (trimmed == "0")
        return false;
    return Strutil::iequals(trimmed, "true") || Strutil::iequals(trimmed, "yes")
           || Strutil::iequals(trimmed, "on");
}
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
                  PlaceholderUiState* ui_state, const std::string& path,
                  int requested_subimage, int requested_miplevel)
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
    if (ui_state != nullptr && should_reset_preview_on_load(viewer, path))
        reset_per_image_preview_state(*ui_state);
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
    add_loaded_image_path(viewer, viewer.image.path, &loaded_index);
    if (!previous_path.empty() && previous_index >= 0
        && previous_path != viewer.image.path
        && previous_index != loaded_index) {
        viewer.last_path_index = previous_index;
    }
    viewer.current_path_index = loaded_index;
    add_recent_image_path(viewer, viewer.image.path);
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

std::string
parent_directory_for_dialog(const std::string& path)
{
    if (path.empty())
        return std::string();
    std::filesystem::path p(path);
    if (!p.has_parent_path())
        return std::string();
    return p.parent_path().string();
}

std::string
open_dialog_default_path(const ViewerState& viewer)
{
    if (!viewer.image.path.empty())
        return parent_directory_for_dialog(viewer.image.path);
    if (!viewer.recent_images.empty())
        return parent_directory_for_dialog(viewer.recent_images.front());
    return std::string();
}

std::string
save_dialog_default_name(const ViewerState& viewer)
{
    if (viewer.image.path.empty())
        return "image.exr";
    std::filesystem::path p(viewer.image.path);
    if (p.filename().empty())
        return "image.exr";
    return p.filename().string();
}

bool
save_loaded_image(const LoadedImage& image, const std::string& path,
                  std::string& error_message)
{
    error_message.clear();
    if (path.empty()) {
        error_message = "save path is empty";
        return false;
    }
    if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
        error_message = "no valid image is loaded";
        return false;
    }

    const TypeDesc format = upload_data_type_to_typedesc(image.type);
    if (format == TypeUnknown) {
        error_message = "unsupported source pixel type for save";
        return false;
    }

    const size_t width         = static_cast<size_t>(image.width);
    const size_t height        = static_cast<size_t>(image.height);
    const size_t channels      = static_cast<size_t>(image.nchannels);
    const size_t min_row_pitch = width * channels * image.channel_bytes;
    if (image.row_pitch_bytes < min_row_pitch) {
        error_message = "image row pitch is invalid";
        return false;
    }
    const size_t required_bytes = image.row_pitch_bytes * height;
    if (image.pixels.size() < required_bytes) {
        error_message = "image pixel buffer is incomplete";
        return false;
    }

    ImageSpec spec(image.width, image.height, image.nchannels, format);
    ImageBuf output(spec);

    const std::byte* begin = reinterpret_cast<const std::byte*>(
        image.pixels.data());
    const cspan<std::byte> byte_span(begin, image.pixels.size());
    const stride_t xstride = static_cast<stride_t>(image.nchannels
                                                   * image.channel_bytes);
    const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
    if (!output.set_pixels(ROI::All(), format, byte_span, begin, xstride,
                           ystride, AutoStride)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "failed to copy pixels into save buffer";
        return false;
    }

    if (!output.write(path, format)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "image write failed";
        return false;
    }
    return true;
}

void
save_as_dialog_action(ViewerState& viewer)
{
    if (viewer.image.path.empty()) {
        viewer.last_error = "No image loaded to save";
        return;
    }

    const std::string default_path = open_dialog_default_path(viewer);
    const std::string default_name = save_dialog_default_name(viewer);
    FileDialog::DialogReply reply  = FileDialog::save_image_file(default_path,
                                                                 default_name);
    if (reply.result == FileDialog::Result::Okay) {
        std::string error;
        if (save_loaded_image(viewer.image, reply.path, error)) {
            add_recent_image_path(viewer, reply.path);
            viewer.status_message = Strutil::fmt::format("Saved {}",
                                                         reply.path);
            viewer.last_error.clear();
        } else {
            viewer.last_error = Strutil::fmt::format("save failed: {}", error);
        }
    } else if (reply.result == FileDialog::Result::Cancel) {
        viewer.status_message = "Save cancelled";
        viewer.last_error.clear();
    } else {
        viewer.last_error = reply.message.empty() ? "Save dialog failed"
                                                  : reply.message;
    }
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
save_window_as_dialog_action(ViewerState& viewer)
{
    save_as_dialog_action(viewer);
}

void
save_selection_as_dialog_action(ViewerState& viewer)
{
    save_as_dialog_action(viewer);
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
set_sort_mode_action(ViewerState& viewer, ImageSortMode mode)
{
    viewer.sort_mode = mode;
    sort_loaded_image_paths(viewer);
    viewer.status_message = "Image list sort mode changed";
    viewer.last_error.clear();
}

void
toggle_sort_reverse_action(ViewerState& viewer)
{
    viewer.sort_reverse = !viewer.sort_reverse;
    sort_loaded_image_paths(viewer);
    viewer.status_message = viewer.sort_reverse ? "Image list order reversed"
                                                : "Image list order restored";
    viewer.last_error.clear();
}

bool
advance_slide_show_action(RendererState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state)
{
    if (!ui_state.slide_show_running || viewer.loaded_image_paths.empty()
        || viewer.image.path.empty()) {
        return false;
    }

    const int count = static_cast<int>(viewer.loaded_image_paths.size());
    if (count <= 0 || viewer.current_path_index < 0)
        return false;

    if (!ui_state.slide_loop && viewer.current_path_index >= count - 1) {
        ui_state.slide_show_running = false;
        viewer.status_message       = "Slide show reached final image";
        viewer.last_error.clear();
        return false;
    }

    std::string next_path;
    if (!pick_loaded_image_path(viewer, 1, next_path) || next_path.empty())
        return false;
    return load_viewer_image(vk_state, viewer, &ui_state, next_path,
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
open_image_dialog_action(RendererState& vk_state, ViewerState& viewer,
                         PlaceholderUiState& ui_state, int requested_subimage,
                         int requested_miplevel)
{
    FileDialog::DialogReply reply = FileDialog::open_image_files(
        open_dialog_default_path(viewer));
    if (reply.result == FileDialog::Result::Okay) {
        if (reply.paths.empty() && !reply.path.empty())
            reply.paths.push_back(reply.path);

        int first_added_index = -1;
        append_loaded_image_paths(viewer, reply.paths, &first_added_index);

        std::string target_path;
        if (first_added_index >= 0
            && first_added_index
                   < static_cast<int>(viewer.loaded_image_paths.size())) {
            target_path = viewer.loaded_image_paths[static_cast<size_t>(
                first_added_index)];
        } else {
            for (const std::string& path : reply.paths) {
                if (!set_current_loaded_image_path(viewer, path))
                    continue;
                if (viewer.current_path_index < 0
                    || viewer.current_path_index >= static_cast<int>(
                           viewer.loaded_image_paths.size())) {
                    continue;
                }
                target_path = viewer.loaded_image_paths[static_cast<size_t>(
                    viewer.current_path_index)];
                break;
            }
        }

        if (!target_path.empty()) {
            load_viewer_image(vk_state, viewer, &ui_state, target_path,
                              requested_subimage, requested_miplevel);
        } else {
            viewer.last_error = "No selected image paths were accepted";
        }
    } else if (reply.result == FileDialog::Result::Cancel) {
        viewer.status_message = "Open cancelled";
        viewer.last_error.clear();
    } else {
        viewer.last_error = reply.message;
    }
}

void
reload_current_image_action(RendererState& vk_state, ViewerState& viewer,
                            PlaceholderUiState& ui_state)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
close_current_image_action(RendererState& vk_state, ViewerState& viewer,
                           PlaceholderUiState& ui_state)
{
    const std::string closing_path = viewer.image.path;
    const int closing_index        = viewer.current_path_index;
    quiesce_viewer_texture_lifetime(vk_state, viewer.texture);
    renderer_destroy_texture(vk_state, viewer.texture);
    remove_loaded_image_path(viewer, closing_path);
    if (!viewer.loaded_image_paths.empty()) {
        const int replacement_index
            = std::clamp(closing_index, 0,
                         static_cast<int>(viewer.loaded_image_paths.size())
                             - 1);
        const std::string replacement_path
            = viewer.loaded_image_paths[static_cast<size_t>(replacement_index)];
        clear_loaded_image_state(viewer);
        viewer.last_error.clear();
        load_viewer_image(vk_state, viewer, &ui_state, replacement_path,
                          ui_state.subimage_index, ui_state.miplevel_index);
        return;
    }

    clear_loaded_image_state(viewer);
    viewer.last_error.clear();
    viewer.status_message = "Closed current image";
}

void
next_sibling_image_action(RendererState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state, int delta)
{
    std::string path;
    if (!pick_loaded_image_path(viewer, delta, path)) {
        viewer.status_message = (delta < 0) ? "Previous image unavailable"
                                            : "Next image unavailable";
        viewer.last_error.clear();
        return;
    }
    load_viewer_image(vk_state, viewer, &ui_state, path, viewer.image.subimage,
                      viewer.image.miplevel);
}

void
toggle_image_action(RendererState& vk_state, ViewerState& viewer,
                    PlaceholderUiState& ui_state)
{
    if (viewer.last_path_index < 0
        || viewer.last_path_index
               >= static_cast<int>(viewer.loaded_image_paths.size())) {
        viewer.status_message = "No toggled image available";
        viewer.last_error.clear();
        return;
    }
    const std::string toggle_path
        = viewer.loaded_image_paths[static_cast<size_t>(viewer.last_path_index)];
    load_viewer_image(vk_state, viewer, &ui_state, toggle_path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
change_subimage_action(RendererState& vk_state, ViewerState& viewer,
                       PlaceholderUiState& ui_state, int delta)
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
            ok = load_viewer_image(vk_state, viewer, &ui_state,
                                   viewer.image.path, viewer.image.subimage,
                                   viewer.image.miplevel - 1);
        } else if (viewer.image.subimage > 0) {
            viewer.auto_subimage = false;
            ok = load_viewer_image(vk_state, viewer, &ui_state,
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
            ok = load_viewer_image(vk_state, viewer, &ui_state,
                                   viewer.image.path, 0, 0);
        } else if (viewer.image.miplevel < viewer.image.nmiplevels - 1) {
            ok = load_viewer_image(vk_state, viewer, &ui_state,
                                   viewer.image.path, viewer.image.subimage,
                                   viewer.image.miplevel + 1);
        } else if (viewer.image.subimage < viewer.image.nsubimages - 1) {
            ok = load_viewer_image(vk_state, viewer, &ui_state,
                                   viewer.image.path, viewer.image.subimage + 1,
                                   0);
        }
    }
    if (ok)
        viewer.last_error.clear();
}

void
change_miplevel_action(RendererState& vk_state, ViewerState& viewer,
                       PlaceholderUiState& ui_state, int delta)
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
    load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
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
    if (!load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                           target_subimage, 0)) {
        return false;
    }
    viewer.auto_subimage = auto_mode;
    restore_view_after_subimage_load(viewer, adjusted_zoom, preserved_scroll);
    return true;
}

bool
capture_main_viewport_screenshot_action(RendererState& vk_state,
                                        ViewerState& viewer,
                                        std::string& out_path)
{
    out_path.clear();
    viewer.last_error.clear();

    int width  = std::max(0, vk_state.framebuffer_width);
    int height = std::max(0, vk_state.framebuffer_height);
    if (width <= 0 || height <= 0) {
        viewer.last_error = "screenshot failed: main viewport size is invalid";
        return false;
    }

    const std::filesystem::path output_path = default_screenshot_output_path();
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec) {
        viewer.last_error = Strutil::fmt::format(
            "screenshot failed: could not create output directory '{}': {}",
            output_path.parent_path().string(), ec.message());
        return false;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width)
                                     * static_cast<size_t>(height));
    if (!renderer_screen_capture(ImGui::GetMainViewport()->ID, 0, 0, width,
                                 height, pixels.data(), &vk_state)) {
        viewer.last_error = "screenshot failed: framebuffer readback failed";
        return false;
    }

    ImageSpec spec(width, height, 4, TypeDesc::UINT8);
    ImageBuf output(spec);
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(
        pixels.data());
    if (!output.set_pixels(ROI::All(), TypeDesc::UINT8, bytes)) {
        viewer.last_error = output.geterror().empty()
                                ? "screenshot failed: could not populate image"
                                : Strutil::fmt::format("screenshot failed: {}",
                                                       output.geterror());
        return false;
    }
    if (!output.write(output_path.string())) {
        viewer.last_error = output.geterror().empty()
                                ? "screenshot failed: image write failed"
                                : Strutil::fmt::format("screenshot failed: {}",
                                                       output.geterror());
        return false;
    }

    out_path              = output_path.string();
    viewer.status_message = Strutil::fmt::format("Saved screenshot {}",
                                                 output_path.string());
    viewer.last_error.clear();
    return true;
}

}  // namespace Imiv
