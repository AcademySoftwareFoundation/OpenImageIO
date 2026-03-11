// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_frame.h"

#include "imiv_actions.h"
#include "imiv_drag_drop.h"
#include "imiv_image_view.h"
#include "imiv_menu.h"
#include "imiv_test_engine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>
#endif

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_dockspace_host_title = "imiv DockSpace";
    constexpr const char* k_image_window_title   = "Image";

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

    bool env_flag_is_truthy(const char* name)
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
        return Strutil::iequals(trimmed, "true")
               || Strutil::iequals(trimmed, "yes")
               || Strutil::iequals(trimmed, "on");
    }

}  // namespace



void
glfw_error_callback(int error, const char* description)
{
    print(stderr, "imiv: GLFW error {}: {}\n", error, description);
}
#if defined(IMGUI_ENABLE_TEST_ENGINE)
void
test_engine_json_write_escaped(FILE* f, const char* s)
{
    std::fputc('"', f);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(
             s ? s : "");
         *p; ++p) {
        const unsigned char c = *p;
        switch (c) {
        case '\\': std::fputs("\\\\", f); break;
        case '"': std::fputs("\\\"", f); break;
        case '\b': std::fputs("\\b", f); break;
        case '\f': std::fputs("\\f", f); break;
        case '\n': std::fputs("\\n", f); break;
        case '\r': std::fputs("\\r", f); break;
        case '\t': std::fputs("\\t", f); break;
        default:
            if (c < 0x20)
                std::fprintf(f, "\\u%04x", static_cast<unsigned int>(c));
            else
                std::fputc(static_cast<int>(c), f);
            break;
        }
    }
    std::fputc('"', f);
}

void
test_engine_json_write_vec2(FILE* f, const ImVec2& v)
{
    std::fprintf(f, "[%.3f,%.3f]", static_cast<double>(v.x),
                 static_cast<double>(v.y));
}

bool
write_test_engine_viewer_state_json(const std::filesystem::path& out_path,
                                    void* user_data, std::string& error_message)
{
    error_message.clear();
    const ViewerStateJsonWriteContext* ctx
        = reinterpret_cast<const ViewerStateJsonWriteContext*>(user_data);
    if (ctx == nullptr || ctx->viewer == nullptr || ctx->ui_state == nullptr) {
        error_message = "viewer state is unavailable";
        return false;
    }

    std::error_code ec;
    if (!out_path.parent_path().empty())
        std::filesystem::create_directories(out_path.parent_path(), ec);

    FILE* f = nullptr;
#    if defined(_WIN32)
    if (fopen_s(&f, out_path.string().c_str(), "wb") != 0)
        f = nullptr;
#    else
    f = std::fopen(out_path.string().c_str(), "wb");
#    endif
    if (f == nullptr) {
        error_message = Strutil::fmt::format("failed to open output file: {}",
                                             out_path.string());
        return false;
    }

    const ViewerState& viewer          = *ctx->viewer;
    const PlaceholderUiState& ui_state = *ctx->ui_state;
    int display_width                  = viewer.image.width;
    int display_height                 = viewer.image.height;
    if (!viewer.image.path.empty())
        oriented_image_dimensions(viewer.image, display_width, display_height);

    std::fputs("{\n", f);
    std::fputs("  \"image_loaded\": ", f);
    std::fputs(viewer.image.path.empty() ? "false" : "true", f);
    std::fputs(",\n  \"image_path\": ", f);
    test_engine_json_write_escaped(f, viewer.image.path.c_str());
    std::fputs(",\n  \"zoom\": ", f);
    std::fprintf(f, "%.6f", static_cast<double>(viewer.zoom));
    std::fputs(",\n  \"scroll\": ", f);
    test_engine_json_write_vec2(f, viewer.scroll);
    std::fputs(",\n  \"norm_scroll\": ", f);
    test_engine_json_write_vec2(f, viewer.norm_scroll);
    std::fputs(",\n  \"max_scroll\": ", f);
    test_engine_json_write_vec2(f, viewer.max_scroll);
    std::fputs(",\n  \"fit_image_to_window\": ", f);
    std::fputs(ui_state.fit_image_to_window ? "true" : "false", f);
    std::fputs(",\n  \"loaded_image_count\": ", f);
    std::fprintf(f, "%d", static_cast<int>(viewer.loaded_image_paths.size()));
    std::fputs(",\n  \"current_image_index\": ", f);
    std::fprintf(f, "%d", viewer.current_path_index);
    std::fputs(",\n  \"drag_overlay_active\": ", f);
    std::fputs(viewer.drag_overlay_active ? "true" : "false", f);
    std::fputs(",\n  \"area_probe_drag_active\": ", f);
    std::fputs(viewer.area_probe_drag_active ? "true" : "false", f);
    std::fputs(",\n  \"image_size\": [", f);
    std::fprintf(f, "%d,%d", viewer.image.width, viewer.image.height);
    std::fputs("],\n  \"display_size\": [", f);
    std::fprintf(f, "%d,%d", display_width, display_height);
    std::fputs("],\n  \"orientation\": ", f);
    std::fprintf(f, "%d", viewer.image.orientation);
    std::fputs(",\n  \"area_probe_lines\": [", f);
    for (size_t i = 0; i < viewer.area_probe_lines.size(); ++i) {
        if (i > 0)
            std::fputs(", ", f);
        test_engine_json_write_escaped(f, viewer.area_probe_lines[i].c_str());
    }
    std::fputs("]", f);
    std::fputs("\n}\n", f);
    std::fflush(f);
    std::fclose(f);
    return true;
}
#endif



ImGuiID
begin_main_dockspace_host()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_flags
        = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
          | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
          | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus
          | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(k_dockspace_host_title, nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID("imiv.main.dockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
    return dockspace_id;
}



void
setup_image_window_policy(ImGuiID dockspace_id, bool force_dock)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowDockID(dockspace_id, force_dock
                                                 ? ImGuiCond_Always
                                                 : ImGuiCond_FirstUseEver);

    ImGuiWindowClass window_class;
    window_class.ClassId                  = ImGui::GetID("imiv.image.window");
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoUndocking
                                            | ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&window_class);
}



bool
primary_monitor_workarea(int& x, int& y, int& w, int& h)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor == nullptr)
        return false;
#if defined(GLFW_VERSION_MAJOR)  \
    && ((GLFW_VERSION_MAJOR > 3) \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    if (w > 0 && h > 0)
        return true;
#endif
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode == nullptr)
        return false;
    x = 0;
    y = 0;
    w = mode->width;
    h = mode->height;
    return (w > 0 && h > 0);
}

bool
centered_glfw_window_pos(GLFWwindow* window, int& out_pos_x, int& out_pos_y,
                         int& out_window_w, int& out_window_h)
{
    out_pos_x    = 0;
    out_pos_y    = 0;
    out_window_w = 0;
    out_window_h = 0;
    if (window == nullptr)
        return false;
    int monitor_x = 0;
    int monitor_y = 0;
    int monitor_w = 0;
    int monitor_h = 0;
    if (!primary_monitor_workarea(monitor_x, monitor_y, monitor_w, monitor_h)) {
        return false;
    }
    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return false;
    int frame_left   = 0;
    int frame_top    = 0;
    int frame_right  = 0;
    int frame_bottom = 0;
    glfwGetWindowFrameSize(window, &frame_left, &frame_top, &frame_right,
                           &frame_bottom);
    const int outer_w = window_w + frame_left + frame_right;
    const int outer_h = window_h + frame_top + frame_bottom;
    out_pos_x = monitor_x + std::max(0, (monitor_w - outer_w) / 2) + frame_left;
    out_pos_y = monitor_y + std::max(0, (monitor_h - outer_h) / 2) + frame_top;
    out_window_w = window_w;
    out_window_h = window_h;
    return true;
}

void
center_glfw_window(GLFWwindow* window)
{
    int pos_x    = 0;
    int pos_y    = 0;
    int window_w = 0;
    int window_h = 0;
    if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w, window_h)) {
        return;
    }
    glfwSetWindowPos(window, pos_x, pos_y);
}

void
force_center_glfw_window(GLFWwindow* window)
{
    int pos_x    = 0;
    int pos_y    = 0;
    int window_w = 0;
    int window_h = 0;
    if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w, window_h)) {
        return;
    }
    glfwSetWindowMonitor(window, nullptr, pos_x, pos_y, window_w, window_h,
                         GLFW_DONT_CARE);
    glfwSetWindowPos(window, pos_x, pos_y);
}

void
draw_viewer_ui(ViewerState& viewer, PlaceholderUiState& ui_state,
               const AppFonts& fonts, bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
               ,
               bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW)
               ,
               GLFWwindow* window, VulkanState& vk_state
#endif
)
{
    reset_layout_dump_synthetic_items();
    reset_test_engine_mouse_space();
    ViewerFrameActions actions;

#if defined(IMIV_BACKEND_VULKAN_GLFW)
    if (window != nullptr) {
        std::string fullscreen_error;
        set_full_screen_mode(window, viewer, ui_state.full_screen_mode,
                             fullscreen_error);
        if (!fullscreen_error.empty()) {
            viewer.last_error         = fullscreen_error;
            ui_state.full_screen_mode = viewer.fullscreen_applied;
        }
    }
#endif

    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_INFO"))
        ui_state.show_info_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREFS"))
        ui_state.show_preferences_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREVIEW"))
        ui_state.show_preview_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PIXEL"))
        ui_state.show_pixelview_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AREA"))
        ui_state.show_area_probe_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AUX_WINDOWS")) {
        ui_state.show_info_window        = true;
        ui_state.show_preferences_window = true;
        ui_state.show_preview_window     = true;
        ui_state.show_pixelview_window   = true;
        ui_state.show_area_probe_window  = true;
    }
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_DRAG_OVERLAY"))
        viewer.drag_overlay_active = true;

    collect_viewer_shortcuts(viewer, ui_state, actions, request_exit);
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    const bool show_test_menu = show_test_engine_windows != nullptr
                                && env_flag_is_truthy(
                                    "IMIV_IMGUI_TEST_ENGINE_SHOW_MENU");
    draw_viewer_main_menu(viewer, ui_state, actions, request_exit,
                          show_test_menu, show_test_engine_windows);
#else
    draw_viewer_main_menu(viewer, ui_state, actions, request_exit);
#endif
    execute_viewer_frame_actions(viewer, ui_state, actions
#if defined(IMIV_BACKEND_VULKAN_GLFW)
                                 ,
                                 window, vk_state
#endif
    );
#if defined(IMIV_BACKEND_VULKAN_GLFW)
    process_pending_drop_paths(vk_state, viewer, ui_state);
#endif
    clamp_placeholder_ui_state(ui_state);

    if (!viewer.image.path.empty()) {
        ui_state.subimage_index = viewer.image.subimage;
        ui_state.miplevel_index = viewer.image.miplevel;
    } else {
        ui_state.subimage_index = 0;
        ui_state.miplevel_index = 0;
    }

#if defined(IMIV_BACKEND_VULKAN_GLFW)
    if (!viewer.image.path.empty()) {
        PreviewControls preview_controls      = {};
        preview_controls.exposure             = ui_state.exposure;
        preview_controls.gamma                = ui_state.gamma;
        preview_controls.offset               = ui_state.offset;
        preview_controls.color_mode           = ui_state.color_mode;
        preview_controls.channel              = ui_state.current_channel;
        preview_controls.use_ocio             = ui_state.use_ocio ? 1 : 0;
        preview_controls.orientation          = viewer.image.orientation;
        preview_controls.linear_interpolation = ui_state.linear_interpolation
                                                    ? 1
                                                    : 0;
        std::string preview_error;
        if (!update_preview_texture(vk_state, viewer.texture, &viewer.image,
                                    ui_state, preview_controls,
                                    preview_error)) {
            if (!preview_error.empty())
                viewer.last_error = preview_error;
        }
    }
#endif

    const ImGuiID main_dockspace_id = begin_main_dockspace_host();
    setup_image_window_policy(main_dockspace_id,
                              ui_state.image_window_force_dock);
    ImGuiWindowFlags main_window_flags = ImGuiWindowFlags_NoCollapse
                                         | ImGuiWindowFlags_NoScrollbar
                                         | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(k_image_window_title, nullptr, main_window_flags);
    ImGui::PopStyleVar();
    ui_state.image_window_force_dock = !ImGui::IsWindowDocked();
    draw_image_window_contents(viewer, ui_state, fonts, actions.pending_zoom,
                               actions.recenter_requested);
    ImGui::End();

    draw_info_window(viewer, ui_state.show_info_window);
    draw_preferences_window(ui_state, ui_state.show_preferences_window);
    draw_preview_window(ui_state, ui_state.show_preview_window);

    if (ImGui::BeginPopupModal("About imiv", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("imiv (Dear ImGui port of iv)");
        register_layout_dump_synthetic_item("text", "About imiv title");
        ImGui::TextUnformatted(
            "Image viewer port built with Dear ImGui and Vulkan.");
        register_layout_dump_synthetic_item("text", "About imiv body");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    draw_drag_drop_overlay(viewer);
}

const char*
image_window_title()
{
    return k_image_window_title;
}

#if defined(IMIV_BACKEND_VULKAN_GLFW)
void
set_glfw_error_callback()
{
    glfwSetErrorCallback(glfw_error_callback);
}
#endif

}  // namespace Imiv
