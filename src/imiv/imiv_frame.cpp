// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_frame.h"

#include "imiv_actions.h"
#include "imiv_backend.h"
#include "imiv_drag_drop.h"
#include "imiv_image_view.h"
#include "imiv_menu.h"
#include "imiv_ocio.h"
#include "imiv_test_engine.h"
#include "imiv_ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

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
        if (!read_env_value(name, value))
            return fallback;
        const std::string trimmed = std::string(Strutil::strip(value));
        if (trimmed.empty())
            return fallback;
        char* end   = nullptr;
        long parsed = std::strtol(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() || *end != '\0')
            return fallback;
        if (parsed < static_cast<long>(std::numeric_limits<int>::min())
            || parsed > static_cast<long>(std::numeric_limits<int>::max())) {
            return fallback;
        }
        return static_cast<int>(parsed);
    }

    void apply_test_engine_ocio_overrides(PlaceholderUiState& ui_state)
    {
        const int apply_frame
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_OCIO_APPLY_FRAME", 0);
        if (ImGui::GetFrameCount() < apply_frame)
            return;

        std::string value;
        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_USE", value)) {
            const string_view trimmed = Strutil::strip(value);
            if (trimmed == "1" || Strutil::iequals(trimmed, "true")
                || Strutil::iequals(trimmed, "yes")
                || Strutil::iequals(trimmed, "on")) {
                ui_state.use_ocio = true;
            } else if (trimmed == "0" || Strutil::iequals(trimmed, "false")
                       || Strutil::iequals(trimmed, "no")
                       || Strutil::iequals(trimmed, "off")) {
                ui_state.use_ocio = false;
            }
        }

        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY", value)) {
            ui_state.use_ocio     = true;
            ui_state.ocio_display = std::string(Strutil::strip(value));
        }
        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW", value)) {
            ui_state.use_ocio  = true;
            ui_state.ocio_view = std::string(Strutil::strip(value));
        }
        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE",
                           value)) {
            ui_state.use_ocio               = true;
            ui_state.ocio_image_color_space = std::string(
                Strutil::strip(value));
        }
    }

    void begin_developer_screenshot_request(DeveloperUiState& developer_ui,
                                            ViewerState& viewer)
    {
#if !defined(NDEBUG)
        if (!developer_ui.request_screenshot || developer_ui.screenshot_busy)
            return;
        developer_ui.request_screenshot  = false;
        developer_ui.screenshot_busy     = true;
        developer_ui.screenshot_due_time = ImGui::GetTime() + 3.0;
        viewer.last_error.clear();
        viewer.status_message
            = "Screenshot queued; capturing main window in 3 seconds";
        print("imiv: developer screenshot queued (3 second delay)\n");
#else
        (void)developer_ui;
        (void)viewer;
#endif
    }

    void draw_developer_windows(DeveloperUiState& developer_ui)
    {
#if !defined(NDEBUG)
        if (developer_ui.show_imgui_demo_window)
            ImGui::ShowDemoWindow(&developer_ui.show_imgui_demo_window);
        if (developer_ui.show_imgui_style_editor) {
            if (ImGui::Begin("Dear ImGui Style Editor",
                             &developer_ui.show_imgui_style_editor)) {
                ImGui::ShowStyleEditor();
            }
            ImGui::End();
        }
        if (developer_ui.show_imgui_metrics_window) {
            ImGui::ShowMetricsWindow(&developer_ui.show_imgui_metrics_window);
        }
        if (developer_ui.show_imgui_debug_log_window) {
            ImGui::ShowDebugLogWindow(
                &developer_ui.show_imgui_debug_log_window);
        }
        if (developer_ui.show_imgui_id_stack_window) {
            ImGui::ShowIDStackToolWindow(
                &developer_ui.show_imgui_id_stack_window);
        }
        if (developer_ui.show_imgui_about_window) {
            ImGui::ShowAboutWindow(&developer_ui.show_imgui_about_window);
        }
#else
        (void)developer_ui;
#endif
    }

}  // namespace



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

void
test_engine_json_write_string_array(FILE* f,
                                    const std::vector<std::string>& values)
{
    std::fputc('[', f);
    for (size_t i = 0, e = values.size(); i < e; ++i) {
        if (i > 0)
            std::fputs(", ", f);
        test_engine_json_write_escaped(f, values[i].c_str());
    }
    std::fputc(']', f);
}

void
test_engine_json_write_ocio_state(FILE* f, const PlaceholderUiState& ui_state)
{
    OcioConfigSelection config_selection;
    resolve_ocio_config_selection(ui_state, config_selection);

    std::vector<std::string> image_color_spaces;
    std::vector<std::string> displays;
    std::vector<std::string> views;
    std::vector<std::pair<std::string, std::vector<std::string>>>
        views_by_display;
    std::string resolved_display;
    std::string resolved_view;
    std::string menu_error;
    const bool menu_data_ok
        = query_ocio_menu_data(ui_state, image_color_spaces, displays, views,
                               resolved_display, resolved_view, menu_error);

    if (menu_data_ok) {
        views_by_display.reserve(displays.size());
        for (const std::string& display_name : displays) {
            PlaceholderUiState probe_state = ui_state;
            probe_state.ocio_display       = display_name;
            probe_state.ocio_view          = "default";

            std::vector<std::string> probe_color_spaces;
            std::vector<std::string> probe_displays;
            std::vector<std::string> probe_views;
            std::string probe_resolved_display;
            std::string probe_resolved_view;
            std::string probe_error;
            if (query_ocio_menu_data(probe_state, probe_color_spaces,
                                     probe_displays, probe_views,
                                     probe_resolved_display,
                                     probe_resolved_view, probe_error)) {
                views_by_display.emplace_back(display_name,
                                              std::move(probe_views));
            } else {
                views_by_display.emplace_back(display_name,
                                              std::vector<std::string>());
            }
        }
    }

    std::fputs(",\n  \"ocio\": {\n", f);
    std::fputs("    \"use_ocio\": ", f);
    std::fputs(ui_state.use_ocio ? "true" : "false", f);
    std::fputs(",\n    \"requested_source\": ", f);
    test_engine_json_write_escaped(f, ocio_config_source_name(
                                          config_selection.requested_source));
    std::fputs(",\n    \"resolved_source\": ", f);
    test_engine_json_write_escaped(f, ocio_config_source_name(
                                          config_selection.resolved_source));
    std::fputs(",\n    \"fallback_applied\": ", f);
    std::fputs(config_selection.fallback_applied ? "true" : "false", f);
    std::fputs(",\n    \"resolved_config_path\": ", f);
    test_engine_json_write_escaped(f, config_selection.resolved_path.c_str());
    std::fputs(",\n    \"display\": ", f);
    test_engine_json_write_escaped(f, ui_state.ocio_display.c_str());
    std::fputs(",\n    \"view\": ", f);
    test_engine_json_write_escaped(f, ui_state.ocio_view.c_str());
    std::fputs(",\n    \"image_color_space\": ", f);
    test_engine_json_write_escaped(f, ui_state.ocio_image_color_space.c_str());
    std::fputs(",\n    \"resolved_display\": ", f);
    test_engine_json_write_escaped(f, resolved_display.c_str());
    std::fputs(",\n    \"resolved_view\": ", f);
    test_engine_json_write_escaped(f, resolved_view.c_str());
    std::fputs(",\n    \"menu_data_ok\": ", f);
    std::fputs(menu_data_ok ? "true" : "false", f);
    std::fputs(",\n    \"menu_error\": ", f);
    test_engine_json_write_escaped(f, menu_error.c_str());
    std::fputs(",\n    \"available_image_color_spaces\": ", f);
    test_engine_json_write_string_array(f, image_color_spaces);
    std::fputs(",\n    \"available_displays\": ", f);
    test_engine_json_write_string_array(f, displays);
    std::fputs(",\n    \"available_views\": ", f);
    test_engine_json_write_string_array(f, views);
    std::fputs(",\n    \"views_by_display\": {\n", f);
    for (size_t i = 0, e = views_by_display.size(); i < e; ++i) {
        if (i > 0)
            std::fputs(",\n", f);
        std::fputs("      ", f);
        test_engine_json_write_escaped(f, views_by_display[i].first.c_str());
        std::fputs(": ", f);
        test_engine_json_write_string_array(f, views_by_display[i].second);
    }
    std::fputs("\n    }\n  }", f);
}

void
test_engine_json_write_backend_state(FILE* f, const PlaceholderUiState& ui_state,
                                     BackendKind active_backend)
{
    const BackendKind requested_backend = sanitize_backend_kind(
        ui_state.renderer_backend);
    const BackendKind next_launch_backend = resolve_backend_request(
        requested_backend);
    const bool requested_backend_compiled
        = requested_backend == BackendKind::Auto
          || backend_kind_is_compiled(requested_backend);

    std::vector<std::string> compiled_backends;
    std::vector<std::string> unavailable_backends;
    compiled_backends.reserve(compiled_backend_info().size());
    unavailable_backends.reserve(compiled_backend_info().size());
    for (const BackendInfo& info : compiled_backend_info()) {
        if (info.compiled)
            compiled_backends.emplace_back(info.cli_name);
        else
            unavailable_backends.emplace_back(info.cli_name);
    }

    std::fputs(",\n  \"backend\": {\n", f);
    std::fputs("    \"active\": ", f);
    test_engine_json_write_escaped(f, backend_cli_name(active_backend));
    std::fputs(",\n    \"active_runtime\": ", f);
    test_engine_json_write_escaped(f, backend_runtime_name(active_backend));
    std::fputs(",\n    \"requested\": ", f);
    test_engine_json_write_escaped(f, backend_cli_name(requested_backend));
    std::fputs(",\n    \"next_launch\": ", f);
    test_engine_json_write_escaped(f, backend_cli_name(next_launch_backend));
    std::fputs(",\n    \"requested_compiled\": ", f);
    std::fputs(requested_backend_compiled ? "true" : "false", f);
    std::fputs(",\n    \"restart_required\": ", f);
    std::fputs(next_launch_backend != active_backend ? "true" : "false", f);
    std::fputs(",\n    \"compiled\": ", f);
    test_engine_json_write_string_array(f, compiled_backends);
    std::fputs(",\n    \"not_compiled\": ", f);
    test_engine_json_write_string_array(f, unavailable_backends);
    std::fputs("\n  }", f);
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
    std::fputs(",\n  \"auto_subimage\": ", f);
    std::fputs(viewer.auto_subimage ? "true" : "false", f);
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
    std::fputs(",\n  \"subimage\": ", f);
    std::fprintf(f, "%d", viewer.image.subimage);
    std::fputs(",\n  \"miplevel\": ", f);
    std::fprintf(f, "%d", viewer.image.miplevel);
    std::fputs(",\n  \"drag_overlay_active\": ", f);
    std::fputs(viewer.drag_overlay_active ? "true" : "false", f);
    std::fputs(",\n  \"area_probe_drag_active\": ", f);
    std::fputs(viewer.area_probe_drag_active ? "true" : "false", f);
    std::fputs(",\n  \"selection_active\": ", f);
    std::fputs(has_image_selection(viewer) ? "true" : "false", f);
    std::fputs(",\n  \"selection_bounds\": [", f);
    std::fprintf(f, "%d,%d,%d,%d", viewer.selection_xbegin,
                 viewer.selection_ybegin, viewer.selection_xend,
                 viewer.selection_yend);
    std::fputs("]", f);
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
    test_engine_json_write_backend_state(f, ui_state, ctx->active_backend);
    test_engine_json_write_ocio_state(f, ui_state);
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
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&window_class);
}



void
draw_viewer_ui(ViewerState& viewer, PlaceholderUiState& ui_state,
               DeveloperUiState& developer_ui, const AppFonts& fonts,
               bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
               ,
               bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
               ,
               GLFWwindow* window, RendererState& vk_state
#endif
)
{
    reset_layout_dump_synthetic_items();
    reset_test_engine_mouse_space();
    ViewerFrameActions actions;

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
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

    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_DRAG_OVERLAY"))
        viewer.drag_overlay_active = true;
    apply_test_engine_ocio_overrides(ui_state);
    set_area_sample_enabled(viewer, ui_state, ui_state.show_area_probe_window);

    collect_viewer_shortcuts(viewer, ui_state, developer_ui, actions,
                             request_exit);
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    const bool show_test_menu = show_test_engine_windows != nullptr
                                && env_flag_is_truthy(
                                    "IMIV_IMGUI_TEST_ENGINE_SHOW_MENU");
    draw_viewer_main_menu(viewer, ui_state, developer_ui, actions, request_exit,
                          show_test_menu, show_test_engine_windows);
#else
    draw_viewer_main_menu(viewer, ui_state, developer_ui, actions,
                          request_exit);
#endif
    begin_developer_screenshot_request(developer_ui, viewer);
    execute_viewer_frame_actions(viewer, ui_state, actions
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
                                 ,
                                 window, vk_state
#endif
    );
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
    process_pending_drop_paths(vk_state, viewer, ui_state);
    (void)apply_pending_auto_subimage_action(vk_state, viewer, ui_state);
#endif
    clamp_placeholder_ui_state(ui_state);

    if (!viewer.image.path.empty()) {
        ui_state.subimage_index = viewer.image.subimage;
        ui_state.miplevel_index = viewer.image.miplevel;
    } else {
        ui_state.subimage_index = 0;
        ui_state.miplevel_index = 0;
    }

    if (!viewer.image.path.empty()) {
        RendererPreviewControls preview_controls = {};
        preview_controls.exposure                = ui_state.exposure;
        preview_controls.gamma                   = ui_state.gamma;
        preview_controls.offset                  = ui_state.offset;
        preview_controls.color_mode              = ui_state.color_mode;
        preview_controls.channel                 = ui_state.current_channel;
        preview_controls.use_ocio                = ui_state.use_ocio ? 1 : 0;
        preview_controls.orientation             = viewer.image.orientation;
        preview_controls.linear_interpolation    = ui_state.linear_interpolation
                                                       ? 1
                                                       : 0;
        std::string preview_error;
        if (!renderer_update_preview_texture(vk_state, viewer.texture,
                                             &viewer.image, ui_state,
                                             preview_controls, preview_error)) {
            if (!preview_error.empty())
                viewer.last_error = preview_error;
        }
    }

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
    draw_preferences_window(ui_state, ui_state.show_preferences_window,
                            renderer_active_backend(vk_state));
    draw_preview_window(ui_state, ui_state.show_preview_window);

    if (ImGui::BeginPopupModal("About imiv", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("imiv (Dear ImGui port of iv)");
        register_layout_dump_synthetic_item("text", "About imiv title");
        ImGui::TextUnformatted("Image viewer port built with Dear ImGui.");
        register_layout_dump_synthetic_item("text", "About imiv body");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    draw_developer_windows(developer_ui);
    draw_drag_drop_overlay(viewer);
}

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
void
process_developer_post_render_actions(DeveloperUiState& developer_ui,
                                      ViewerState& viewer,
                                      RendererState& vk_state)
{
#    if !defined(NDEBUG)
    if (!developer_ui.screenshot_busy)
        return;
    if (ImGui::GetTime() < developer_ui.screenshot_due_time)
        return;

    std::string out_path;
    if (!capture_main_viewport_screenshot_action(vk_state, viewer, out_path)) {
        if (viewer.last_error.empty())
            viewer.last_error = "screenshot capture failed";
        print(stderr, "imiv: developer screenshot failed: {}\n",
              viewer.last_error);
    } else {
        print("imiv: developer screenshot saved {}\n", out_path);
    }
    developer_ui.screenshot_busy     = false;
    developer_ui.screenshot_due_time = -1.0;
#    else
    (void)developer_ui;
    (void)viewer;
    (void)vk_state;
#    endif
}
#endif

const char*
image_window_title()
{
    return k_image_window_title;
}

}  // namespace Imiv
