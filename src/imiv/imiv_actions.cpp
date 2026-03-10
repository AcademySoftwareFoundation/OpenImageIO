// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_actions.h"

#include "imiv_file_dialog.h"
#include "imiv_ui.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
#    define GLFW_INCLUDE_NONE
#    include <GLFW/glfw3.h>
#endif

#include <imgui.h>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

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
#if defined(IMIV_BACKEND_VULKAN_GLFW)
bool
viewer_texture_has_gpu_lifetime(const VulkanTexture& texture)
{
    return texture.image != VK_NULL_HANDLE
           || texture.source_image != VK_NULL_HANDLE
           || texture.set != VK_NULL_HANDLE
           || texture.nearest_mag_set != VK_NULL_HANDLE
           || texture.pixelview_set != VK_NULL_HANDLE
           || texture.upload_submit_pending || texture.preview_submit_pending;
}

void
quiesce_viewer_texture_lifetime(VulkanState& vk_state,
                                const VulkanTexture& texture)
{
    if (vk_state.device == VK_NULL_HANDLE
        || !viewer_texture_has_gpu_lifetime(texture)) {
        return;
    }
    VkResult err = vkDeviceWaitIdle(vk_state.device);
    check_vk_result(err);
}
#endif

bool
load_viewer_image(VulkanState& vk_state, ViewerState& viewer,
                  PlaceholderUiState* ui_state, const std::string& path,
                  int requested_subimage, int requested_miplevel)
{
    viewer.last_error.clear();
    LoadedImage loaded;
    std::string error;
    if (!load_image_for_compute(path, requested_subimage, requested_miplevel,
                                loaded, error)) {
        viewer.last_error = Strutil::fmt::format("open failed: {}", error);
        print(stderr, "imiv: {}\n", viewer.last_error);
        return false;
    }
    VulkanTexture texture;
    if (!create_texture(vk_state, loaded, texture, error)) {
        viewer.last_error = Strutil::fmt::format("upload failed: {}", error);
        print(stderr, "imiv: {}\n", viewer.last_error);
        return false;
    }
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    quiesce_viewer_texture_lifetime(vk_state, viewer.texture);
#endif
    destroy_texture(vk_state, viewer.texture);
    if (!viewer.image.path.empty())
        viewer.toggle_image_path = viewer.image.path;
    if (ui_state != nullptr && should_reset_preview_on_load(viewer, path))
        reset_per_image_preview_state(*ui_state);
    viewer.image       = std::move(loaded);
    viewer.texture     = std::move(texture);
    viewer.zoom        = 1.0f;
    viewer.fit_request = true;
    reset_view_navigation_state(viewer);
    viewer.probe_valid = false;
    viewer.probe_channels.clear();
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
    add_recent_image_path(viewer, viewer.image.path);
    refresh_sibling_images(viewer);
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

#if defined(IMIV_BACKEND_VULKAN_GLFW)
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
set_sort_mode_action(ViewerState& viewer, ImageSortMode mode)
{
    viewer.sort_mode = mode;
    sort_sibling_images(viewer);
    viewer.status_message = "Image list sort mode changed";
    viewer.last_error.clear();
}

void
toggle_sort_reverse_action(ViewerState& viewer)
{
    viewer.sort_reverse = !viewer.sort_reverse;
    sort_sibling_images(viewer);
    viewer.status_message = viewer.sort_reverse ? "Image list order reversed"
                                                : "Image list order restored";
    viewer.last_error.clear();
}

bool
advance_slide_show_action(VulkanState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state)
{
    if (!ui_state.slide_show_running || viewer.sibling_images.empty()
        || viewer.image.path.empty()) {
        return false;
    }

    const int count = static_cast<int>(viewer.sibling_images.size());
    if (count <= 0 || viewer.sibling_index < 0)
        return false;

    if (!ui_state.slide_loop && viewer.sibling_index >= count - 1) {
        ui_state.slide_show_running = false;
        viewer.status_message       = "Slide show reached final image";
        viewer.last_error.clear();
        return false;
    }

    std::string next_path;
    if (!pick_sibling_image(viewer, 1, next_path) || next_path.empty())
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
open_image_dialog_action(VulkanState& vk_state, ViewerState& viewer,
                         PlaceholderUiState& ui_state, int requested_subimage,
                         int requested_miplevel)
{
    FileDialog::DialogReply reply = FileDialog::open_image_file(
        open_dialog_default_path(viewer));
    if (reply.result == FileDialog::Result::Okay) {
        load_viewer_image(vk_state, viewer, &ui_state, reply.path,
                          requested_subimage, requested_miplevel);
    } else if (reply.result == FileDialog::Result::Cancel) {
        viewer.status_message = "Open cancelled";
        viewer.last_error.clear();
    } else {
        viewer.last_error = reply.message;
    }
}

void
reload_current_image_action(VulkanState& vk_state, ViewerState& viewer,
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
close_current_image_action(VulkanState& vk_state, ViewerState& viewer)
{
#    if defined(IMIV_BACKEND_VULKAN_GLFW)
    quiesce_viewer_texture_lifetime(vk_state, viewer.texture);
#    endif
    destroy_texture(vk_state, viewer.texture);
    if (!viewer.image.path.empty())
        viewer.toggle_image_path = viewer.image.path;
    viewer.image       = LoadedImage();
    viewer.zoom        = 1.0f;
    viewer.fit_request = true;
    reset_view_navigation_state(viewer);
    viewer.probe_valid = false;
    viewer.probe_channels.clear();
    viewer.sibling_images.clear();
    viewer.sibling_index = -1;
    viewer.last_error.clear();
    viewer.status_message = "Closed current image";
}

void
next_sibling_image_action(VulkanState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state, int delta)
{
    std::string path;
    if (!pick_sibling_image(viewer, delta, path)) {
        viewer.status_message = (delta < 0) ? "Previous image unavailable"
                                            : "Next image unavailable";
        viewer.last_error.clear();
        return;
    }
    load_viewer_image(vk_state, viewer, &ui_state, path, viewer.image.subimage,
                      viewer.image.miplevel);
}

void
toggle_image_action(VulkanState& vk_state, ViewerState& viewer,
                    PlaceholderUiState& ui_state)
{
    if (viewer.toggle_image_path.empty()) {
        viewer.status_message = "No toggled image available";
        viewer.last_error.clear();
        return;
    }
    if (viewer.image.path == viewer.toggle_image_path) {
        if (!pick_sibling_image(viewer, 1, viewer.toggle_image_path)) {
            viewer.status_message = "No toggled image available";
            viewer.last_error.clear();
            return;
        }
    }
    load_viewer_image(vk_state, viewer, &ui_state, viewer.toggle_image_path,
                      viewer.image.subimage, viewer.image.miplevel);
}

void
change_subimage_action(VulkanState& vk_state, ViewerState& viewer,
                       PlaceholderUiState& ui_state, int delta)
{
    if (viewer.image.path.empty()) {
        viewer.status_message = "No image loaded";
        viewer.last_error.clear();
        return;
    }
    const int target_subimage = viewer.image.subimage + delta;
    if (target_subimage < 0 || target_subimage >= viewer.image.nsubimages)
        return;
    load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                      target_subimage, viewer.image.miplevel);
}

void
change_miplevel_action(VulkanState& vk_state, ViewerState& viewer,
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
    load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                      viewer.image.subimage, target_mip);
}

#endif

}  // namespace Imiv
