// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_frame.h"

#include "imiv_actions.h"
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

}  // namespace

bool
apply_forced_probe_from_env(ViewerState& viewer)
{
    const int forced_x = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_X", -1);
    const int forced_y = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_Y", -1);
    if (forced_x < 0 || forced_y < 0 || viewer.image.path.empty())
        return false;

    const int px = std::clamp(forced_x, 0, std::max(0, viewer.image.width - 1));
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
    std::fputs(",\n  \"image_size\": [", f);
    std::fprintf(f, "%d,%d", viewer.image.width, viewer.image.height);
    std::fputs("],\n  \"display_size\": [", f);
    std::fprintf(f, "%d,%d", display_width, display_height);
    std::fputs("],\n  \"orientation\": ", f);
    std::fprintf(f, "%d", viewer.image.orientation);
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
current_child_visible_rect(const ImVec2& padding, bool scroll_x, bool scroll_y,
                           ImVec2& out_min, ImVec2& out_max)
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



bool
app_shortcut(ImGuiKeyChord key_chord)
{
    return ImGui::Shortcut(key_chord,
                           ImGuiInputFlags_RouteGlobal
                               | ImGuiInputFlags_RouteUnlessBgFocused);
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

    bool open_requested                = false;
    bool save_as_requested             = false;
    bool clear_recent_requested        = false;
    bool reload_requested              = false;
    bool close_requested               = false;
    bool prev_requested                = false;
    bool next_requested                = false;
    bool toggle_requested              = false;
    bool prev_subimage_requested       = false;
    bool next_subimage_requested       = false;
    bool prev_mip_requested            = false;
    bool next_mip_requested            = false;
    bool save_window_as_requested      = false;
    bool save_selection_as_requested   = false;
    bool fit_window_to_image_requested = false;
    bool recenter_requested            = false;
    bool delete_from_disk_requested    = false;
    bool full_screen_toggle_requested  = false;
    bool rotate_left_requested         = false;
    bool rotate_right_requested        = false;
    bool flip_horizontal_requested     = false;
    bool flip_vertical_requested       = false;
    PendingZoomRequest pending_zoom;
    std::string recent_open_path;
    const bool has_image         = !viewer.image.path.empty();
    const bool can_prev_subimage = has_image && viewer.image.subimage > 0;
    const bool can_next_subimage
        = has_image && (viewer.image.subimage + 1 < viewer.image.nsubimages);
    const bool can_prev_mip = has_image && viewer.image.miplevel > 0;
    const bool can_next_mip
        = has_image && (viewer.image.miplevel + 1 < viewer.image.nmiplevels);

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

    const ImGuiIO& global_io = ImGui::GetIO();
    const bool no_mods       = !global_io.KeyCtrl && !global_io.KeyAlt
                         && !global_io.KeySuper;

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

    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
        open_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_R) && has_image)
        reload_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_W) && has_image)
        close_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_S) && has_image)
        save_as_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Comma))
        ui_state.show_preferences_window = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Q))
        request_exit = true;
    if (app_shortcut(ImGuiKey_PageUp))
        prev_requested = true;
    if (app_shortcut(ImGuiKey_PageDown))
        next_requested = true;
    if (no_mods && app_shortcut(ImGuiKey_T))
        toggle_requested = true;
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd))
        && has_image)
        request_zoom_scale(pending_zoom, 2.0f, false);
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract))
        && has_image)
        request_zoom_scale(pending_zoom, 0.5f, false);
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_0)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Keypad0))
        && has_image)
        request_zoom_reset(pending_zoom, false);
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Period)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadDecimal))
        && has_image)
        recenter_requested = true;
    if (no_mods && app_shortcut(ImGuiKey_F) && has_image)
        fit_window_to_image_requested = true;
    if (app_shortcut(ImGuiMod_Alt | ImGuiKey_F) && has_image) {
        ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
        viewer.fit_request           = true;
    }
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_F))
        full_screen_toggle_requested = true;
    if (ui_state.full_screen_mode && app_shortcut(ImGuiKey_Escape))
        full_screen_toggle_requested = true;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Comma) && can_prev_subimage)
        prev_subimage_requested = true;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Period) && can_next_subimage)
        next_subimage_requested = true;
    if (no_mods && app_shortcut(ImGuiKey_C))
        ui_state.current_channel = 0;
    if (no_mods && app_shortcut(ImGuiKey_R))
        ui_state.current_channel = 1;
    if (no_mods && app_shortcut(ImGuiKey_G))
        ui_state.current_channel = 2;
    if (no_mods && app_shortcut(ImGuiKey_B))
        ui_state.current_channel = 3;
    if (no_mods && app_shortcut(ImGuiKey_A))
        ui_state.current_channel = 4;
    if (no_mods && app_shortcut(ImGuiKey_Comma) && has_image)
        ui_state.current_channel = std::max(0, ui_state.current_channel - 1);
    if (no_mods && app_shortcut(ImGuiKey_Period) && has_image)
        ui_state.current_channel = std::min(4, ui_state.current_channel + 1);
    if (no_mods && app_shortcut(ImGuiKey_1))
        ui_state.color_mode = 2;
    if (no_mods && app_shortcut(ImGuiKey_L))
        ui_state.color_mode = 3;
    if (no_mods && app_shortcut(ImGuiKey_H))
        ui_state.color_mode = 4;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_I))
        ui_state.show_info_window = !ui_state.show_info_window;
    if (no_mods && app_shortcut(ImGuiKey_P))
        ui_state.show_pixelview_window = !ui_state.show_pixelview_window;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
        ui_state.show_area_probe_window = !ui_state.show_area_probe_window;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_LeftBracket))
        ui_state.exposure -= 0.5f;
    if (app_shortcut(ImGuiKey_LeftBracket))
        ui_state.exposure -= 0.1f;
    if (app_shortcut(ImGuiKey_RightBracket))
        ui_state.exposure += 0.1f;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_RightBracket))
        ui_state.exposure += 0.5f;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_9))
        ui_state.gamma = std::max(0.1f, ui_state.gamma - 0.1f);
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_0))
        ui_state.gamma += 0.1f;
    if (app_shortcut(ImGuiKey_Delete) && has_image)
        delete_from_disk_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_L))
        rotate_left_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_R))
        rotate_right_requested = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
                open_requested = true;

            if (ImGui::BeginMenu("Open recent...")) {
                if (viewer.recent_images.empty()) {
                    ImGui::MenuItem("No recent files", nullptr, false, false);
                } else {
                    for (size_t i = 0; i < viewer.recent_images.size(); ++i) {
                        const std::string& recent = viewer.recent_images[i];
                        const std::string label
                            = Strutil::fmt::format("{}: {}##imiv_recent_{}",
                                                   i + 1, recent, i);
                        if (ImGui::MenuItem(label.c_str()))
                            recent_open_path = recent;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear recent list", nullptr, false,
                                    !viewer.recent_images.empty()))
                    clear_recent_requested = true;
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Reload image", "Ctrl+R", false, has_image))
                reload_requested = true;
            if (ImGui::MenuItem("Close image", "Ctrl+W", false, has_image))
                close_requested = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Save As...", "Ctrl+S", false, has_image))
                save_as_requested = true;
            if (ImGui::MenuItem("Save Window As...", nullptr, false, has_image))
                save_window_as_requested = true;
            if (ImGui::MenuItem("Save Selection As...", nullptr, false,
                                has_image))
                save_selection_as_requested = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Move to new window", nullptr, false, has_image))
                viewer.status_message
                    = "Move to new window is not available in imiv yet";
            if (ImGui::MenuItem("Delete from disk", "Delete", false, has_image))
                delete_from_disk_requested = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...", "Ctrl+,"))
                ui_state.show_preferences_window = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Ctrl+Q"))
                request_exit = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Previous Image", "PgUp"))
                prev_requested = true;
            if (ImGui::MenuItem("Next Image", "PgDown"))
                next_requested = true;
            if (ImGui::MenuItem("Toggle image", "T"))
                toggle_requested = true;
            ImGui::MenuItem("Show display/data window borders", nullptr,
                            &ui_state.show_window_guides);
            ImGui::Separator();

            if (ImGui::MenuItem("Zoom In", "Ctrl++", false, has_image))
                request_zoom_scale(pending_zoom, 2.0f, false);
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, has_image))
                request_zoom_scale(pending_zoom, 0.5f, false);
            if (ImGui::MenuItem("Normal Size (1:1)", "Ctrl+0", false, has_image))
                request_zoom_reset(pending_zoom, false);
            if (ImGui::MenuItem("Re-center Image", "Ctrl+.", false, has_image))
                recenter_requested = true;
            if (ImGui::MenuItem("Fit Window to Image", "F", false, has_image))
                fit_window_to_image_requested = true;
            if (ImGui::MenuItem("Fit Image to Window", "Alt+F",
                                ui_state.fit_image_to_window, has_image)) {
                ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
                viewer.fit_request           = true;
            }
            if (ImGui::MenuItem("Full screen", "Ctrl+F",
                                ui_state.full_screen_mode)) {
                full_screen_toggle_requested = true;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Prev Subimage", "<", false, can_prev_subimage))
                prev_subimage_requested = true;
            if (ImGui::MenuItem("Next Subimage", ">", false, can_next_subimage))
                next_subimage_requested = true;
            if (ImGui::MenuItem("Prev MIP level", nullptr, false, can_prev_mip))
                prev_mip_requested = true;
            if (ImGui::MenuItem("Next MIP level", nullptr, false, can_next_mip))
                next_mip_requested = true;

            if (ImGui::BeginMenu("Channels")) {
                if (ImGui::MenuItem("Full Color", "C",
                                    ui_state.current_channel == 0))
                    ui_state.current_channel = 0;
                if (ImGui::MenuItem("Red", "R", ui_state.current_channel == 1))
                    ui_state.current_channel = 1;
                if (ImGui::MenuItem("Green", "G", ui_state.current_channel == 2))
                    ui_state.current_channel = 2;
                if (ImGui::MenuItem("Blue", "B", ui_state.current_channel == 3))
                    ui_state.current_channel = 3;
                if (ImGui::MenuItem("Alpha", "A", ui_state.current_channel == 4))
                    ui_state.current_channel = 4;
                ImGui::Separator();
                if (ImGui::MenuItem("Prev Channel", ",", false, has_image)) {
                    ui_state.current_channel
                        = std::max(0, ui_state.current_channel - 1);
                }
                if (ImGui::MenuItem("Next Channel", ".", false, has_image)) {
                    ui_state.current_channel
                        = std::min(4, ui_state.current_channel + 1);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Color mode")) {
                if (ImGui::MenuItem("RGBA", nullptr, ui_state.color_mode == 0))
                    ui_state.color_mode = 0;
                if (ImGui::MenuItem("RGB", nullptr, ui_state.color_mode == 1))
                    ui_state.color_mode = 1;
                if (ImGui::MenuItem("Single channel", "1",
                                    ui_state.color_mode == 2))
                    ui_state.color_mode = 2;
                if (ImGui::MenuItem("Luminance", "L", ui_state.color_mode == 3))
                    ui_state.color_mode = 3;
                if (ImGui::MenuItem("Single channel (Heatmap)", "H",
                                    ui_state.color_mode == 4))
                    ui_state.color_mode = 4;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("OCIO")) {
                ImGui::MenuItem("Use OCIO", nullptr, &ui_state.use_ocio);
                if (ImGui::BeginMenu("Image color space")) {
                    if (ImGui::MenuItem("auto", nullptr,
                                        ui_state.ocio_image_color_space
                                            == "auto"))
                        ui_state.ocio_image_color_space = "auto";
                    if (ImGui::MenuItem("scene_linear", nullptr,
                                        ui_state.ocio_image_color_space
                                            == "scene_linear"))
                        ui_state.ocio_image_color_space = "scene_linear";
                    if (ImGui::MenuItem("sRGB", nullptr,
                                        ui_state.ocio_image_color_space
                                            == "sRGB"))
                        ui_state.ocio_image_color_space = "sRGB";
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Display/View")) {
                    if (ImGui::MenuItem("default / default", nullptr,
                                        ui_state.ocio_display == "default"
                                            && ui_state.ocio_view
                                                   == "default")) {
                        ui_state.ocio_display = "default";
                        ui_state.ocio_view    = "default";
                    }
                    if (ImGui::MenuItem("sRGB / Film", nullptr,
                                        ui_state.ocio_display == "sRGB"
                                            && ui_state.ocio_view == "Film")) {
                        ui_state.ocio_display = "sRGB";
                        ui_state.ocio_view    = "Film";
                    }
                    if (ImGui::MenuItem("sRGB / Raw", nullptr,
                                        ui_state.ocio_display == "sRGB"
                                            && ui_state.ocio_view == "Raw")) {
                        ui_state.ocio_display = "sRGB";
                        ui_state.ocio_view    = "Raw";
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Exposure/gamma")) {
                if (ImGui::MenuItem("Exposure -1/2 stop", "{"))
                    ui_state.exposure -= 0.5f;
                if (ImGui::MenuItem("Exposure -1/10 stop", "["))
                    ui_state.exposure -= 0.1f;
                if (ImGui::MenuItem("Exposure +1/10 stop", "]"))
                    ui_state.exposure += 0.1f;
                if (ImGui::MenuItem("Exposure +1/2 stop", "}"))
                    ui_state.exposure += 0.5f;
                if (ImGui::MenuItem("Gamma -0.1", "("))
                    ui_state.gamma = std::max(0.1f, ui_state.gamma - 0.1f);
                if (ImGui::MenuItem("Gamma +0.1", ")"))
                    ui_state.gamma += 0.1f;
                if (ImGui::MenuItem("Reset exposure/gamma")) {
                    ui_state.exposure = 0.0f;
                    ui_state.gamma    = 1.0f;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Image info...", "Ctrl+I",
                            &ui_state.show_info_window);
            ImGui::MenuItem("Preview controls...", nullptr,
                            &ui_state.show_preview_window);
            ImGui::MenuItem("Pixel closeup view...", "P",
                            &ui_state.show_pixelview_window);
            ImGui::MenuItem("Toggle area sample", "Ctrl+A",
                            &ui_state.show_area_probe_window);

            if (ImGui::BeginMenu("Slide Show")) {
                if (ImGui::MenuItem("Start Slide Show", nullptr,
                                    ui_state.slide_show_running)) {
                    toggle_slide_show_action(ui_state, viewer);
                }
                if (ImGui::MenuItem("Loop slide show", nullptr,
                                    ui_state.slide_loop))
                    ui_state.slide_loop = !ui_state.slide_loop;
                if (ImGui::MenuItem("Stop at end", nullptr,
                                    !ui_state.slide_loop))
                    ui_state.slide_loop = false;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Sort")) {
                if (ImGui::MenuItem("By Name"))
                    set_sort_mode_action(viewer, ImageSortMode::ByName);
                if (ImGui::MenuItem("By File Path"))
                    set_sort_mode_action(viewer, ImageSortMode::ByPath);
                if (ImGui::MenuItem("By Image Date"))
                    set_sort_mode_action(viewer, ImageSortMode::ByImageDate);
                if (ImGui::MenuItem("By File Date"))
                    set_sort_mode_action(viewer, ImageSortMode::ByFileDate);
                if (ImGui::MenuItem("Reverse current order"))
                    toggle_sort_reverse_action(viewer);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Rotate Left", "Ctrl+Shift+L"))
                rotate_left_requested = true;
            if (ImGui::MenuItem("Rotate Right", "Ctrl+Shift+R"))
                rotate_right_requested = true;
            if (ImGui::MenuItem("Flip Horizontal"))
                flip_horizontal_requested = true;
            if (ImGui::MenuItem("Flip Vertical"))
                flip_vertical_requested = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About"))
                ImGui::OpenPopup("About imiv");
            ImGui::EndMenu();
        }

#if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (show_test_engine_windows
            && env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_MENU")
            && ImGui::BeginMenu("Tests")) {
            ImGui::MenuItem("Show test engine windows", nullptr,
                            show_test_engine_windows);
            ImGui::EndMenu();
        }
#endif
        ImGui::EndMainMenuBar();
    }

    if (open_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        open_image_dialog_action(vk_state, viewer, ui_state,
                                 ui_state.subimage_index,
                                 ui_state.miplevel_index);
#else
        set_placeholder_status(viewer, "Open image");
#endif
        open_requested = false;
    }
    if (!recent_open_path.empty()) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        load_viewer_image(vk_state, viewer, &ui_state, recent_open_path,
                          ui_state.subimage_index, ui_state.miplevel_index);
#else
        set_placeholder_status(viewer, "Open recent image");
#endif
        recent_open_path.clear();
    }
    if (clear_recent_requested) {
        viewer.recent_images.clear();
        viewer.status_message = "Cleared recent files list";
        viewer.last_error.clear();
        clear_recent_requested = false;
    }
    if (reload_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        reload_current_image_action(vk_state, viewer, ui_state);
#else
        set_placeholder_status(viewer, "Reload image");
#endif
        reload_requested = false;
    }
    if (close_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        close_current_image_action(vk_state, viewer);
#else
        set_placeholder_status(viewer, "Close image");
#endif
        close_requested = false;
    }
    if (prev_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        next_sibling_image_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Previous Image");
#endif
        prev_requested = false;
    }
    if (next_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        next_sibling_image_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next Image");
#endif
        next_requested = false;
    }
    if (toggle_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        toggle_image_action(vk_state, viewer, ui_state);
#else
        set_placeholder_status(viewer, "Toggle image");
#endif
        toggle_requested = false;
    }
    if (prev_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_subimage_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Prev Subimage");
#endif
        prev_subimage_requested = false;
    }
    if (next_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_subimage_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next Subimage");
#endif
        next_subimage_requested = false;
    }
    if (prev_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_miplevel_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Prev MIP level");
#endif
        prev_mip_requested = false;
    }
    if (next_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_miplevel_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next MIP level");
#endif
        next_mip_requested = false;
    }
    if (save_as_requested) {
        save_as_dialog_action(viewer);
        save_as_requested = false;
    }
    if (save_window_as_requested) {
        save_window_as_dialog_action(viewer);
        save_window_as_requested = false;
    }
    if (save_selection_as_requested) {
        save_selection_as_dialog_action(viewer);
        save_selection_as_requested = false;
    }
    if (fit_window_to_image_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        fit_window_to_image_action(window, viewer, ui_state);
#else
        viewer.status_message = "Fit window to image is unavailable";
#endif
        fit_window_to_image_requested = false;
    }
    if (full_screen_toggle_requested) {
        ui_state.full_screen_mode = !ui_state.full_screen_mode;
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        std::string fullscreen_error;
        set_full_screen_mode(window, viewer, ui_state.full_screen_mode,
                             fullscreen_error);
        if (!fullscreen_error.empty()) {
            viewer.last_error         = fullscreen_error;
            ui_state.full_screen_mode = viewer.fullscreen_applied;
        } else {
            viewer.status_message = ui_state.full_screen_mode
                                        ? "Entered full screen"
                                        : "Exited full screen";
            viewer.last_error.clear();
        }
#endif
        full_screen_toggle_requested = false;
    }
    if (delete_from_disk_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        if (!viewer.image.path.empty()) {
            const std::string to_delete = viewer.image.path;
            close_current_image_action(vk_state, viewer);
            std::error_code ec;
            if (std::filesystem::remove(to_delete, ec)) {
                viewer.status_message = Strutil::fmt::format("Deleted {}",
                                                             to_delete);
                viewer.last_error.clear();
                refresh_sibling_images(viewer);
            } else {
                viewer.last_error
                    = ec ? Strutil::fmt::format("Delete failed: {}",
                                                ec.message())
                         : "Delete failed";
            }
        }
#endif
        delete_from_disk_requested = false;
    }
    if (rotate_left_requested || rotate_right_requested
        || flip_horizontal_requested || flip_vertical_requested) {
        if (!viewer.image.path.empty()) {
            int orientation = clamp_orientation(viewer.image.orientation);
            if (rotate_left_requested) {
                static const int next_orientation[] = { 0, 8, 5, 6, 7,
                                                        4, 1, 2, 3 };
                orientation = next_orientation[orientation];
            }
            if (rotate_right_requested) {
                static const int next_orientation[] = { 0, 6, 7, 8, 5,
                                                        2, 3, 4, 1 };
                orientation = next_orientation[orientation];
            }
            if (flip_horizontal_requested) {
                static const int next_orientation[] = { 0, 2, 1, 4, 3,
                                                        6, 5, 8, 7 };
                orientation = next_orientation[orientation];
            }
            if (flip_vertical_requested) {
                static const int next_orientation[] = { 0, 4, 3, 2, 1,
                                                        8, 7, 6, 5 };
                orientation = next_orientation[orientation];
            }
            viewer.image.orientation = clamp_orientation(orientation);
            viewer.fit_request       = true;
            viewer.status_message
                = Strutil::fmt::format("Orientation set to {}",
                                       viewer.image.orientation);
            viewer.last_error.clear();
        } else {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
        }
        rotate_left_requested     = false;
        rotate_right_requested    = false;
        flip_horizontal_requested = false;
        flip_vertical_requested   = false;
    }

#if defined(IMIV_BACKEND_VULKAN_GLFW)
    if (ui_state.slide_show_running && has_image
        && !viewer.sibling_images.empty()) {
        const double now = ImGui::GetTime();
        if (viewer.slide_last_advance_time <= 0.0)
            viewer.slide_last_advance_time = now;
        const double delay = std::max(1, ui_state.slide_duration_seconds);
        if (now - viewer.slide_last_advance_time >= delay) {
            (void)advance_slide_show_action(vk_state, viewer, ui_state);
            viewer.slide_last_advance_time = now;
        }
    } else {
        viewer.slide_last_advance_time = 0.0;
    }
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
        if (!update_preview_texture(vk_state, viewer.texture, preview_controls,
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

    if (!viewer.image.path.empty()) {
        const ImVec2 image_size = image_layout.image_size;
        ImTextureRef main_texture_ref;
        ImTextureRef closeup_texture_ref;
        bool has_main_texture     = false;
        bool has_closeup_texture  = false;
        bool image_canvas_pressed = false;
        bool image_canvas_hovered = false;
        bool image_canvas_active  = false;

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
        update_test_engine_mouse_space(coord_map.viewport_rect_min,
                                       coord_map.viewport_rect_max,
                                       (has_image && coord_map.valid)
                                           ? coord_map.image_rect_min
                                           : ImVec2(0.0f, 0.0f),
                                       (has_image && coord_map.valid)
                                           ? coord_map.image_rect_max
                                           : ImVec2(0.0f, 0.0f));
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

        const ImGuiIO& io         = ImGui::GetIO();
        const ImVec2 mouse        = io.MousePos;
        const bool mouse_in_image = point_in_rect(mouse,
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

        bool want_pan       = false;
        bool want_zoom_drag = false;
        if (viewport_accepts_mouse || image_canvas_accepts_mouse
            || viewer.pan_drag_active || viewer.zoom_drag_active) {
            if (ui_state.mouse_mode == 1) {
                want_pan = ImGui::IsMouseDown(ImGuiMouseButton_Left)
                           || ImGui::IsMouseDown(ImGuiMouseButton_Right)
                           || ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            } else if (ui_state.mouse_mode == 0) {
                const bool want_middle_pan
                    = ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                      && (viewer.pan_drag_active || image_canvas_accepts_mouse
                          || viewport_accepts_mouse);
                const bool want_alt_left_pan
                    = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)
                      && (viewer.pan_drag_active || image_canvas_accepts_mouse
                          || viewport_accepts_mouse);
                want_pan       = want_middle_pan || want_alt_left_pan;
                want_zoom_drag = io.KeyAlt
                                 && ImGui::IsMouseDown(ImGuiMouseButton_Right)
                                 && (viewer.zoom_drag_active
                                     || image_canvas_accepts_mouse
                                     || viewport_accepts_mouse);
                if (!io.KeyAlt
                    && (image_canvas_clicked_left || image_canvas_clicked_right
                        || empty_viewport_clicked_left
                        || empty_viewport_clicked_right)) {
                    if (image_canvas_clicked_left
                        || empty_viewport_clicked_left)
                        request_zoom_scale(pending_zoom, 2.0f, true);
                    if (image_canvas_clicked_right
                        || empty_viewport_clicked_right)
                        request_zoom_scale(pending_zoom, 0.5f, true);
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
