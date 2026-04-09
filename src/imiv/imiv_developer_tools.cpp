// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_developer_tools.h"

#include "imiv_file_actions.h"
#include "imiv_ocio.h"
#include "imiv_parse.h"
#include "imiv_workspace_ui.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

void
apply_test_engine_ocio_overrides(PlaceholderUiState& ui_state)
{
    const int apply_frame
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_OCIO_APPLY_FRAME", 0);
    if (ImGui::GetFrameCount() < apply_frame)
        return;

    std::string value;
    env_read_bool_value("IMIV_IMGUI_TEST_ENGINE_OCIO_USE", ui_state.use_ocio);

    if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY", value)) {
        ui_state.use_ocio     = true;
        ui_state.ocio_display = std::string(Strutil::strip(value));
    }
    if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW", value)) {
        ui_state.use_ocio  = true;
        ui_state.ocio_view = std::string(Strutil::strip(value));
    }
    if (read_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE", value)) {
        ui_state.use_ocio               = true;
        ui_state.ocio_image_color_space = std::string(Strutil::strip(value));
    }

    env_read_bool_value("IMIV_IMGUI_TEST_ENGINE_LINEAR_INTERPOLATION",
                        ui_state.linear_interpolation);
}



void
apply_test_engine_view_activation_override(MultiViewWorkspace& workspace)
{
    const int apply_frame
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_VIEW_APPLY_FRAME", -1);
    if (apply_frame < 0 || ImGui::GetFrameCount() < apply_frame)
        return;

    const int activate_view_index
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_ACTIVATE_VIEW_INDEX", -1);
    if (activate_view_index >= 0
        && activate_view_index
               < static_cast<int>(workspace.view_windows.size())) {
        ImageViewWindow* target
            = workspace.view_windows[static_cast<size_t>(activate_view_index)]
                  .get();
        if (target != nullptr) {
            workspace.active_view_id = target->id;
            target->request_focus    = true;
        }
    }
}



void
apply_test_engine_view_recipe_overrides(PlaceholderUiState& ui_state)
{
    const int apply_frame
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_VIEW_APPLY_FRAME", -1);
    if (apply_frame < 0 || ImGui::GetFrameCount() < apply_frame)
        return;

    std::string backend_value;
    if (read_env_value("IMIV_IMGUI_TEST_ENGINE_RENDERER_BACKEND",
                       backend_value)) {
        BackendKind backend_kind = BackendKind::Auto;
        if (parse_backend_kind(backend_value, backend_kind)) {
            ui_state.renderer_backend = static_cast<int>(backend_kind);
        }
    }

    bool found           = false;
    const float exposure = env_float_value("IMIV_IMGUI_TEST_ENGINE_EXPOSURE",
                                           ui_state.exposure, &found);
    if (found)
        ui_state.exposure = exposure;

    const float gamma = env_float_value("IMIV_IMGUI_TEST_ENGINE_GAMMA",
                                        ui_state.gamma, &found);
    if (found)
        ui_state.gamma = gamma;

    const float offset = env_float_value("IMIV_IMGUI_TEST_ENGINE_OFFSET",
                                         ui_state.offset, &found);
    if (found)
        ui_state.offset = offset;
}



void
apply_test_engine_drop_overrides(ViewerState& viewer)
{
    const int apply_frame
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_DROP_APPLY_FRAME", -1);
    if (apply_frame < 0 || ImGui::GetFrameCount() != apply_frame)
        return;

    std::string path_file;
    if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_DROP_PATHS_FILE", path_file))
        return;
    path_file = std::string(Strutil::strip(path_file));
    if (path_file.empty())
        return;

    std::ifstream input(path_file);
    if (!input)
        return;

    std::vector<std::string> drop_paths;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = std::string(Strutil::strip(line));
        if (trimmed.empty())
            continue;
        drop_paths.emplace_back(trimmed);
    }

    if (drop_paths.empty())
        return;

    viewer.pending_drop_paths  = std::move(drop_paths);
    viewer.drag_overlay_active = false;
}



void
begin_developer_screenshot_request(DeveloperUiState& developer_ui,
                                   ViewerState& viewer)
{
    if (!developer_ui.enabled)
        return;
    if (!developer_ui.request_screenshot || developer_ui.screenshot_busy)
        return;
    developer_ui.request_screenshot  = false;
    developer_ui.screenshot_busy     = true;
    developer_ui.screenshot_due_time = ImGui::GetTime() + 3.0;
    viewer.last_error.clear();
    viewer.status_message
        = "Screenshot queued; capturing main window in 3 seconds";
    print("imiv: developer screenshot queued (3 second delay)\n");
}



void
draw_developer_windows(DeveloperUiState& developer_ui)
{
    if (!developer_ui.enabled)
        return;
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
        ImGui::ShowDebugLogWindow(&developer_ui.show_imgui_debug_log_window);
    }
    if (developer_ui.show_imgui_id_stack_window) {
        ImGui::ShowIDStackToolWindow(&developer_ui.show_imgui_id_stack_window);
    }
    if (developer_ui.show_imgui_about_window) {
        ImGui::ShowAboutWindow(&developer_ui.show_imgui_about_window);
    }
}



#if defined(IMGUI_ENABLE_TEST_ENGINE)
namespace {

    void test_engine_json_write_escaped(FILE* f, const char* s)
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

    void test_engine_json_write_vec2(FILE* f, const ImVec2& v)
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

    void test_engine_json_write_ocio_state(FILE* f,
                                           const PlaceholderUiState& ui_state)
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
            = query_ocio_menu_data(ui_state, image_color_spaces, displays,
                                   views, resolved_display, resolved_view,
                                   menu_error);

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
        test_engine_json_write_escaped(f,
                                       ocio_config_source_name(
                                           config_selection.requested_source));
        std::fputs(",\n    \"resolved_source\": ", f);
        test_engine_json_write_escaped(f,
                                       ocio_config_source_name(
                                           config_selection.resolved_source));
        std::fputs(",\n    \"fallback_applied\": ", f);
        std::fputs(config_selection.fallback_applied ? "true" : "false", f);
        std::fputs(",\n    \"resolved_config_path\": ", f);
        test_engine_json_write_escaped(f,
                                       config_selection.resolved_path.c_str());
        std::fputs(",\n    \"display\": ", f);
        test_engine_json_write_escaped(f, ui_state.ocio_display.c_str());
        std::fputs(",\n    \"view\": ", f);
        test_engine_json_write_escaped(f, ui_state.ocio_view.c_str());
        std::fputs(",\n    \"image_color_space\": ", f);
        test_engine_json_write_escaped(f,
                                       ui_state.ocio_image_color_space.c_str());
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
            test_engine_json_write_escaped(f,
                                           views_by_display[i].first.c_str());
            std::fputs(": ", f);
            test_engine_json_write_string_array(f, views_by_display[i].second);
        }
        std::fputs("\n    }\n  }", f);
    }

    void test_engine_json_write_backend_state(
        FILE* f, const PlaceholderUiState& ui_state, BackendKind active_backend)
    {
        const BackendKind requested_backend = sanitize_backend_kind(
            ui_state.renderer_backend);
        const BackendKind next_launch_backend = resolve_backend_request(
            requested_backend);
        const bool requested_backend_compiled
            = requested_backend == BackendKind::Auto
              || backend_kind_is_compiled(requested_backend);
        const bool requested_backend_available
            = requested_backend == BackendKind::Auto
              || backend_kind_is_available(requested_backend);

        std::vector<std::string> compiled_backends;
        std::vector<std::string> not_compiled_backends;
        std::vector<std::string> available_backends;
        std::vector<std::string> unavailable_backends;
        compiled_backends.reserve(runtime_backend_info().size());
        not_compiled_backends.reserve(runtime_backend_info().size());
        available_backends.reserve(runtime_backend_info().size());
        unavailable_backends.reserve(runtime_backend_info().size());
        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            if (info.build_info.compiled)
                compiled_backends.emplace_back(info.build_info.cli_name);
            else
                not_compiled_backends.emplace_back(info.build_info.cli_name);
            if (!info.build_info.compiled)
                continue;
            if (info.available)
                available_backends.emplace_back(info.build_info.cli_name);
            else
                unavailable_backends.emplace_back(info.build_info.cli_name);
        }

        std::fputs(",\n  \"backend\": {\n", f);
        std::fputs("    \"active\": ", f);
        test_engine_json_write_escaped(f, backend_cli_name(active_backend));
        std::fputs(",\n    \"active_runtime\": ", f);
        test_engine_json_write_escaped(f, backend_runtime_name(active_backend));
        std::fputs(",\n    \"requested\": ", f);
        test_engine_json_write_escaped(f, backend_cli_name(requested_backend));
        std::fputs(",\n    \"next_launch\": ", f);
        test_engine_json_write_escaped(f,
                                       backend_cli_name(next_launch_backend));
        std::fputs(",\n    \"requested_compiled\": ", f);
        std::fputs(requested_backend_compiled ? "true" : "false", f);
        std::fputs(",\n    \"requested_available\": ", f);
        std::fputs(requested_backend_available ? "true" : "false", f);
        std::fputs(",\n    \"restart_required\": ", f);
        std::fputs(next_launch_backend != active_backend ? "true" : "false", f);
        std::fputs(",\n    \"availability_probed\": ", f);
        std::fputs(runtime_backend_info_valid() ? "true" : "false", f);
        std::fputs(",\n    \"compiled\": ", f);
        test_engine_json_write_string_array(f, compiled_backends);
        std::fputs(",\n    \"available\": ", f);
        test_engine_json_write_string_array(f, available_backends);
        std::fputs(",\n    \"unavailable\": ", f);
        test_engine_json_write_string_array(f, unavailable_backends);
        std::fputs(",\n    \"not_compiled\": ", f);
        test_engine_json_write_string_array(f, not_compiled_backends);
        std::fputs(",\n    \"unavailable_reason\": {\n", f);
        bool first_reason = true;
        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            if (!info.build_info.compiled || info.available)
                continue;
            if (!first_reason)
                std::fputs(",\n", f);
            first_reason = false;
            std::fputs("      ", f);
            test_engine_json_write_escaped(f, info.build_info.cli_name);
            std::fputs(": ", f);
            test_engine_json_write_escaped(f, info.unavailable_reason.c_str());
        }
        std::fputs("\n    }", f);
        std::fputs("\n  }", f);
    }

    void test_engine_json_write_view_recipe_state(FILE* f,
                                                  const ViewerState& viewer)
    {
        std::fputs(",\n  \"view_recipe\": {\n", f);
        std::fputs("    \"use_ocio\": ", f);
        std::fputs(viewer.recipe.use_ocio ? "true" : "false", f);
        std::fputs(",\n    \"linear_interpolation\": ", f);
        std::fputs(viewer.recipe.linear_interpolation ? "true" : "false", f);
        std::fputs(",\n    \"current_channel\": ", f);
        std::fprintf(f, "%d", viewer.recipe.current_channel);
        std::fputs(",\n    \"color_mode\": ", f);
        std::fprintf(f, "%d", viewer.recipe.color_mode);
        std::fputs(",\n    \"exposure\": ", f);
        std::fprintf(f, "%.6f", static_cast<double>(viewer.recipe.exposure));
        std::fputs(",\n    \"gamma\": ", f);
        std::fprintf(f, "%.6f", static_cast<double>(viewer.recipe.gamma));
        std::fputs(",\n    \"offset\": ", f);
        std::fprintf(f, "%.6f", static_cast<double>(viewer.recipe.offset));
        std::fputs(",\n    \"ocio_display\": ", f);
        test_engine_json_write_escaped(f, viewer.recipe.ocio_display.c_str());
        std::fputs(",\n    \"ocio_view\": ", f);
        test_engine_json_write_escaped(f, viewer.recipe.ocio_view.c_str());
        std::fputs(",\n    \"ocio_image_color_space\": ", f);
        test_engine_json_write_escaped(
            f, viewer.recipe.ocio_image_color_space.c_str());
        std::fputs("\n  }", f);
    }

}  // namespace

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

    const ImageViewWindow* state_view = ctx->workspace != nullptr
                                            ? active_image_view(*ctx->workspace)
                                            : nullptr;
    const ViewerState& viewer = (state_view != nullptr) ? state_view->viewer
                                                        : *ctx->viewer;
    const PlaceholderUiState& ui_state = *ctx->ui_state;
    int display_width                  = viewer.image.width;
    int display_height                 = viewer.image.height;
    if (!viewer.image.path.empty())
        oriented_image_dimensions(viewer.image, display_width, display_height);
    ImVec2 viewport_origin(0.0f, 0.0f);
    ImVec2 viewport_size(0.0f, 0.0f);
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        viewport_origin = viewport->Pos;
        viewport_size   = viewport->Size;
    }

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
    std::fputs(",\n  \"view_count\": ", f);
    std::fprintf(f, "%d",
                 ctx->workspace != nullptr
                     ? static_cast<int>(ctx->workspace->view_windows.size())
                     : 1);
    std::fputs(",\n  \"loaded_view_count\": ", f);
    {
        int loaded_view_count = 0;
        if (ctx->workspace != nullptr) {
            for (const std::unique_ptr<ImageViewWindow>& view :
                 ctx->workspace->view_windows) {
                if (view != nullptr && !view->viewer.image.path.empty())
                    ++loaded_view_count;
            }
        } else if (!viewer.image.path.empty()) {
            loaded_view_count = 1;
        }
        std::fprintf(f, "%d", loaded_view_count);
    }
    std::fputs(",\n  \"active_view_id\": ", f);
    std::fprintf(f, "%d",
                 ctx->workspace != nullptr ? ctx->workspace->active_view_id
                                           : 0);
    std::fputs(",\n  \"active_view_docked\": ", f);
    {
        const ImageViewWindow* active_view
            = ctx->workspace != nullptr ? active_image_view(*ctx->workspace)
                                        : nullptr;
        std::fputs((active_view != nullptr && active_view->is_docked) ? "true"
                                                                      : "false",
                   f);
    }
    std::fputs(",\n  \"image_list_visible\": ", f);
    std::fputs((ctx->workspace != nullptr
                && ctx->workspace->show_image_list_window)
                   ? "true"
                   : "false",
               f);
    std::fputs(",\n  \"image_list_drawn\": ", f);
    std::fputs((ctx->workspace != nullptr
                && ctx->workspace->image_list_was_drawn)
                   ? "true"
                   : "false",
               f);
    std::fputs(",\n  \"image_list_docked\": ", f);
    std::fputs((ctx->workspace != nullptr
                && ctx->workspace->image_list_is_docked)
                   ? "true"
                   : "false",
               f);
    std::fputs(",\n  \"image_list_pos\": ", f);
    test_engine_json_write_vec2(f, ctx->workspace != nullptr
                                       ? ctx->workspace->image_list_pos
                                       : ImVec2(0.0f, 0.0f));
    std::fputs(",\n  \"image_list_size\": ", f);
    test_engine_json_write_vec2(f, ctx->workspace != nullptr
                                       ? ctx->workspace->image_list_size
                                       : ImVec2(0.0f, 0.0f));
    std::fputs(",\n  \"image_list_item_rects\": [", f);
    if (ctx->workspace != nullptr) {
        for (size_t i = 0; i < ctx->workspace->image_list_item_rects.size();
             ++i) {
            const ImVec4 rect = ctx->workspace->image_list_item_rects[i];
            if (i > 0)
                std::fputs(", ", f);
            std::fputs("[", f);
            std::fprintf(f, "%.3f,%.3f,%.3f,%.3f", static_cast<double>(rect.x),
                         static_cast<double>(rect.y),
                         static_cast<double>(rect.z),
                         static_cast<double>(rect.w));
            std::fputs("]", f);
        }
    }
    std::fputs("]", f);
    std::fputs(",\n  \"image_list_open_view_ids\": [", f);
    if (ctx->workspace != nullptr) {
        for (size_t i = 0; i < ctx->viewer->loaded_image_paths.size(); ++i) {
            if (i > 0)
                std::fputs(", ", f);
            const std::vector<int> view_ids
                = image_list_open_view_ids(*ctx->workspace,
                                           ctx->viewer->loaded_image_paths[i]);
            std::fputs("[", f);
            for (size_t j = 0; j < view_ids.size(); ++j) {
                if (j > 0)
                    std::fputs(", ", f);
                std::fprintf(f, "%d", view_ids[j]);
            }
            std::fputs("]", f);
        }
    }
    std::fputs("]", f);
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
    std::fputs("]", f);
    std::fputs(",\n  \"viewport_origin\": ", f);
    test_engine_json_write_vec2(f, viewport_origin);
    std::fputs(",\n  \"viewport_size\": ", f);
    test_engine_json_write_vec2(f, viewport_size);
    std::fputs(",\n  \"orientation\": ", f);
    std::fprintf(f, "%d", viewer.image.orientation);
    std::fputs(",\n  \"area_probe_lines\": [", f);
    for (size_t i = 0; i < viewer.area_probe_lines.size(); ++i) {
        if (i > 0)
            std::fputs(", ", f);
        test_engine_json_write_escaped(f, viewer.area_probe_lines[i].c_str());
    }
    std::fputs("]", f);
    test_engine_json_write_view_recipe_state(f, viewer);
    test_engine_json_write_backend_state(f, ui_state, ctx->active_backend);
    test_engine_json_write_ocio_state(f, ui_state);
    std::fputs("\n}\n", f);
    std::fflush(f);
    std::fclose(f);
    return true;
}
#endif



#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
void
process_developer_post_render_actions(DeveloperUiState& developer_ui,
                                      ViewerState& viewer,
                                      RendererState& vk_state)
{
    if (!developer_ui.enabled)
        return;
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
}
#endif

}  // namespace Imiv
