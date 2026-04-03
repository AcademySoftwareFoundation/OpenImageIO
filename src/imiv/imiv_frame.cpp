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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_dockspace_host_title    = "imiv DockSpace";
    constexpr const char* k_image_window_title      = "Image";
    constexpr const char* k_image_list_window_title = "Image List";

    bool consume_focus_request(PlaceholderUiState& ui_state,
                               const char* window_name)
    {
        if (ui_state.focus_window_name == nullptr || window_name == nullptr)
            return false;
        if (std::strcmp(ui_state.focus_window_name, window_name) != 0)
            return false;
        ui_state.focus_window_name = nullptr;
        return true;
    }

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

    bool parse_bool_value(const std::string& value, bool& out)
    {
        const string_view trimmed = Strutil::strip(value);
        if (trimmed == "1" || Strutil::iequals(trimmed, "true")
            || Strutil::iequals(trimmed, "yes")
            || Strutil::iequals(trimmed, "on")) {
            out = true;
            return true;
        }
        if (trimmed == "0" || Strutil::iequals(trimmed, "false")
            || Strutil::iequals(trimmed, "no")
            || Strutil::iequals(trimmed, "off")) {
            out = false;
            return true;
        }
        return false;
    }

    std::string normalize_image_list_path(const std::string& path)
    {
        if (path.empty())
            return std::string();
        std::filesystem::path fs_path(path);
        std::error_code ec;
        if (!fs_path.is_absolute()) {
            const std::filesystem::path abs = std::filesystem::absolute(fs_path,
                                                                        ec);
            if (!ec)
                fs_path = abs;
        }
        return fs_path.lexically_normal().string();
    }

    bool debug_image_list_windows_enabled()
    {
        static int cached_value = -1;
        if (cached_value < 0)
            cached_value = env_flag_is_truthy("IMIV_DEBUG_IMAGE_LIST_WINDOWS")
                               ? 1
                               : 0;
        return cached_value != 0;
    }

    bool image_list_tooltips_disabled()
    {
        static int cached_value = -1;
        if (cached_value < 0) {
            if (env_flag_is_truthy("IMIV_DISABLE_IMAGE_LIST_TOOLTIPS")) {
                cached_value = 1;
            } else {
                std::string ignored;
                cached_value = (read_env_value("WSL_INTEROP", ignored)
                                || read_env_value("WSL_DISTRO_NAME", ignored))
                                   ? 1
                                   : 0;
            }
        }
        return cached_value != 0;
    }

    void update_image_list_visibility_policy(MultiViewWorkspace& workspace,
                                             const ImageLibraryState& library)
    {
        const int image_count = static_cast<int>(
            library.loaded_image_paths.size());
        if (image_count <= 0) {
            workspace.show_image_list_window   = false;
            workspace.image_list_force_dock    = false;
            workspace.last_library_image_count = image_count;
            return;
        }

        if (image_count > 1
            && image_count != workspace.last_library_image_count) {
            workspace.show_image_list_window = true;
            workspace.image_list_force_dock  = true;
        }
        workspace.last_library_image_count = image_count;
    }

    void ensure_image_list_default_layout(MultiViewWorkspace& workspace,
                                          ImGuiID dockspace_id)
    {
        if (!workspace.show_image_list_window
            || workspace.image_list_layout_initialized) {
            return;
        }

        if (ImGui::FindWindowSettingsByID(ImHashStr(k_image_list_window_title))
            != nullptr) {
            workspace.image_list_layout_initialized = true;
            return;
        }

        ImGuiDockNode* dockspace_node = ImGui::DockBuilderGetNode(dockspace_id);
        if (dockspace_node == nullptr || dockspace_node->Size.x <= 0.0f) {
            return;
        }

        const float ratio = std::clamp(200.0f / dockspace_node->Size.x, 0.12f,
                                       0.35f);
        ImGuiID image_list_dock_id = 0;
        ImGuiID image_view_dock_id = dockspace_id;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, ratio,
                                    &image_list_dock_id, &image_view_dock_id);
        if (image_list_dock_id == 0 || image_view_dock_id == 0)
            return;

        workspace.image_view_dock_id    = image_view_dock_id;
        workspace.image_list_dock_id    = image_list_dock_id;
        workspace.image_list_force_dock = true;
        ImGui::DockBuilderDockWindow(k_image_window_title, image_view_dock_id);
        ImGui::DockBuilderDockWindow(k_image_list_window_title,
                                     image_list_dock_id);
        ImGui::DockBuilderFinish(dockspace_id);
        workspace.image_list_layout_initialized = true;
    }

    void reset_window_layouts(MultiViewWorkspace& workspace,
                              PlaceholderUiState& ui_state,
                              ImGuiID dockspace_id)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::ClearIniSettings();
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        if (viewport != nullptr)
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);
        ImGui::DockBuilderFinish(dockspace_id);

        ui_state.image_window_force_dock        = true;
        workspace.image_view_dock_id            = dockspace_id;
        workspace.image_list_dock_id            = 0;
        workspace.image_list_layout_initialized = false;
        workspace.image_list_force_dock = workspace.show_image_list_window;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view != nullptr) {
                view->force_dock    = true;
                view->request_focus = (view->id == workspace.active_view_id);
            }
        }
        ImGui::GetIO().WantSaveIniSettings = true;
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

    float env_float_value(const char* name, float fallback,
                          bool* found = nullptr)
    {
        if (found != nullptr)
            *found = false;
        std::string value;
        if (!read_env_value(name, value))
            return fallback;
        const std::string trimmed = std::string(Strutil::strip(value));
        if (trimmed.empty())
            return fallback;
        char* end    = nullptr;
        float parsed = std::strtof(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || *end != '\0')
            return fallback;
        if (found != nullptr)
            *found = true;
        return parsed;
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

        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_LINEAR_INTERPOLATION",
                           value)) {
            const string_view trimmed = Strutil::strip(value);
            if (trimmed == "1" || Strutil::iequals(trimmed, "true")
                || Strutil::iequals(trimmed, "yes")
                || Strutil::iequals(trimmed, "on")) {
                ui_state.linear_interpolation = true;
            } else if (trimmed == "0" || Strutil::iequals(trimmed, "false")
                       || Strutil::iequals(trimmed, "no")
                       || Strutil::iequals(trimmed, "off")) {
                ui_state.linear_interpolation = false;
            }
        }
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
                = workspace
                      .view_windows[static_cast<size_t>(activate_view_index)]
                      .get();
            if (target != nullptr) {
                workspace.active_view_id = target->id;
                target->request_focus    = true;
            }
        }
    }

    void apply_test_engine_view_recipe_overrides(PlaceholderUiState& ui_state)
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

        bool found = false;
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

    bool activate_image_list_path(MultiViewWorkspace& workspace,
                                  ImageLibraryState& library,
                                  ViewerState& active_view,
                                  PlaceholderUiState& ui_state,
                                  RendererState& renderer_state,
                                  const std::string& path)
    {
        if (!load_viewer_image(renderer_state, active_view, library, &ui_state,
                               path, ui_state.subimage_index,
                               ui_state.miplevel_index)) {
            return false;
        }
        sync_workspace_library_state(workspace, active_view, library);
        return true;
    }

    bool open_image_list_path_in_new_view(MultiViewWorkspace& workspace,
                                          ImageLibraryState& library,
                                          ViewerState& active_view,
                                          PlaceholderUiState& ui_state,
                                          RendererState& renderer_state,
                                          const std::string& path)
    {
        ImageViewWindow& new_view          = append_image_view(workspace);
        new_view.viewer.loaded_image_paths = library.loaded_image_paths;
        new_view.viewer.recent_images      = library.recent_images;
        new_view.viewer.sort_mode          = library.sort_mode;
        new_view.viewer.sort_reverse       = library.sort_reverse;
        new_view.viewer.recipe             = active_view.recipe;
        new_view.request_focus             = true;
        if (!load_viewer_image(renderer_state, new_view.viewer, library,
                               &ui_state, path, ui_state.subimage_index,
                               ui_state.miplevel_index)) {
            return false;
        }
        workspace.active_view_id = new_view.id;
        sync_workspace_library_state(workspace, active_view, library);
        return true;
    }

    bool image_view_is_showing_path(const ViewerState& viewer,
                                    const std::string& normalized_path)
    {
        return !viewer.image.path.empty()
               && normalize_image_list_path(viewer.image.path)
                      == normalized_path;
    }

    int image_list_open_view_count(const MultiViewWorkspace& workspace,
                                   const std::string& normalized_path)
    {
        int count = 0;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view != nullptr
                && image_view_is_showing_path(view->viewer, normalized_path)) {
                ++count;
            }
        }
        return count;
    }

    std::vector<int>
    image_list_open_view_ids(const MultiViewWorkspace& workspace,
                             const std::string& normalized_path)
    {
        std::vector<int> view_ids;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view != nullptr
                && image_view_is_showing_path(view->viewer, normalized_path)) {
                view_ids.push_back(view->id);
            }
        }
        return view_ids;
    }

    std::string image_list_open_view_badge(const MultiViewWorkspace& workspace,
                                           const std::string& normalized_path)
    {
        const std::vector<int> view_ids
            = image_list_open_view_ids(workspace, normalized_path);
        if (view_ids.empty())
            return std::string();
        std::string joined;
        for (size_t i = 0; i < view_ids.size(); ++i) {
            if (i > 0)
                joined += ",";
            joined += Strutil::to_string(view_ids[i]);
        }
        return Strutil::fmt::format(" [v:{}]", joined);
    }

    void adjust_viewer_indices_after_remove(ViewerState& viewer,
                                            int remove_index)
    {
        if (viewer.current_path_index == remove_index) {
            viewer.current_path_index = -1;
        } else if (viewer.current_path_index > remove_index) {
            --viewer.current_path_index;
        }

        if (viewer.last_path_index == remove_index) {
            viewer.last_path_index = -1;
        } else if (viewer.last_path_index > remove_index) {
            --viewer.last_path_index;
        }
    }

    bool close_image_list_path_in_active_view(MultiViewWorkspace& workspace,
                                              ImageLibraryState& library,
                                              ViewerState& active_view,
                                              PlaceholderUiState& ui_state,
                                              RendererState& renderer_state,
                                              const std::string& path)
    {
        const std::string normalized_path = normalize_image_list_path(path);
        if (!image_view_is_showing_path(active_view, normalized_path))
            return false;
        close_current_image_action(renderer_state, active_view, library,
                                   ui_state);
        sync_workspace_library_state(workspace, active_view, library);
        active_view.status_message = Strutil::fmt::format("Closed {}", path);
        active_view.last_error.clear();
        return true;
    }

    bool close_image_list_path_in_all_views(MultiViewWorkspace& workspace,
                                            ImageLibraryState& library,
                                            ViewerState& active_view,
                                            PlaceholderUiState& ui_state,
                                            RendererState& renderer_state,
                                            const std::string& path)
    {
        const std::string normalized_path = normalize_image_list_path(path);
        bool closed_any                   = false;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view == nullptr
                || !image_view_is_showing_path(view->viewer, normalized_path)) {
                continue;
            }
            close_current_image_action(renderer_state, view->viewer, library,
                                       ui_state);
            closed_any = true;
        }
        if (closed_any) {
            active_view.status_message
                = Strutil::fmt::format("Closed {} in all views", path);
            active_view.last_error.clear();
        }
        return closed_any;
    }

    bool remove_image_list_path_from_session(MultiViewWorkspace& workspace,
                                             ImageLibraryState& library,
                                             ViewerState& active_view,
                                             PlaceholderUiState& ui_state,
                                             RendererState& renderer_state,
                                             const std::string& path)
    {
        const bool was_image_list_visible = workspace.show_image_list_window;
        const std::string normalized_path = normalize_image_list_path(path);
        const auto it = std::find(library.loaded_image_paths.begin(),
                                  library.loaded_image_paths.end(),
                                  normalized_path);
        if (it == library.loaded_image_paths.end())
            return false;

        const int remove_index = static_cast<int>(
            std::distance(library.loaded_image_paths.begin(), it));
        std::vector<ImageViewWindow*> affected_views;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view == nullptr
                || !image_view_is_showing_path(view->viewer, normalized_path)) {
                continue;
            }
            affected_views.push_back(view.get());
            close_current_image_action(renderer_state, view->viewer, library,
                                       ui_state);
        }

        library.loaded_image_paths.erase(it);
        std::string replacement_path;
        if (!library.loaded_image_paths.empty()) {
            const int replacement_index
                = std::min(remove_index,
                           static_cast<int>(library.loaded_image_paths.size())
                               - 1);
            replacement_path = library.loaded_image_paths[static_cast<size_t>(
                replacement_index)];
        }
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view == nullptr)
                continue;
            adjust_viewer_indices_after_remove(view->viewer, remove_index);
            view->viewer.loaded_image_paths = library.loaded_image_paths;
            view->viewer.recent_images      = library.recent_images;
            view->viewer.sort_mode          = library.sort_mode;
            view->viewer.sort_reverse       = library.sort_reverse;
        }
        if (!replacement_path.empty()) {
            for (ImageViewWindow* view : affected_views) {
                if (view == nullptr)
                    continue;
                load_viewer_image(renderer_state, view->viewer, library,
                                  &ui_state, replacement_path, 0, 0);
            }
        }
        bool any_view_loaded = false;
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view != nullptr && !view->viewer.image.path.empty()) {
                any_view_loaded = true;
                break;
            }
        }
        if (!replacement_path.empty() && !any_view_loaded) {
            ViewerState& primary_view
                = ensure_primary_image_view(workspace).viewer;
            load_viewer_image(renderer_state, primary_view, library, &ui_state,
                              replacement_path, 0, 0);
        }
        sync_workspace_library_state(workspace, active_view, library);
        update_image_list_visibility_policy(workspace, library);
        if (was_image_list_visible && !library.loaded_image_paths.empty()) {
            workspace.show_image_list_window = true;
        }
        active_view.status_message
            = Strutil::fmt::format("Removed {} from session", path);
        active_view.last_error.clear();
        return true;
    }

    void apply_test_engine_image_list_visibility_override(
        MultiViewWorkspace& workspace)
    {
        const int apply_frame
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_APPLY_FRAME",
                            -1);
        if (apply_frame < 0 || ImGui::GetFrameCount() != apply_frame)
            return;

        std::string show_image_list_value;
        if (read_env_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_VISIBLE",
                           show_image_list_value)) {
            bool visible = false;
            if (parse_bool_value(show_image_list_value, visible)) {
                workspace.show_image_list_window = visible;
                if (visible)
                    workspace.image_list_force_dock = true;
            }
        }
    }

    void apply_test_engine_image_list_overrides(MultiViewWorkspace& workspace,
                                                ImageLibraryState& library,
                                                ViewerState& active_view,
                                                PlaceholderUiState& ui_state,
                                                RendererState& renderer_state)
    {
        const int apply_frame
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_APPLY_FRAME",
                            -1);
        if (apply_frame < 0 || ImGui::GetFrameCount() != apply_frame)
            return;

        const int select_index
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_SELECT_INDEX",
                            -1);
        if (select_index >= 0
            && select_index
                   < static_cast<int>(library.loaded_image_paths.size())) {
            activate_image_list_path(workspace, library, active_view, ui_state,
                                     renderer_state,
                                     library.loaded_image_paths[select_index]);
        }

        const int open_new_view_index = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_OPEN_NEW_VIEW_INDEX", -1);
        if (open_new_view_index >= 0
            && open_new_view_index
                   < static_cast<int>(library.loaded_image_paths.size())) {
            open_image_list_path_in_new_view(
                workspace, library, active_view, ui_state, renderer_state,
                library.loaded_image_paths[open_new_view_index]);
        }

        const int close_active_index = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_CLOSE_ACTIVE_INDEX", -1);
        if (close_active_index >= 0
            && close_active_index
                   < static_cast<int>(library.loaded_image_paths.size())) {
            close_image_list_path_in_active_view(
                workspace, library, active_view, ui_state, renderer_state,
                library.loaded_image_paths[close_active_index]);
        }

        const int remove_index
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_REMOVE_INDEX",
                            -1);
        if (remove_index >= 0
            && remove_index
                   < static_cast<int>(library.loaded_image_paths.size())) {
            remove_image_list_path_from_session(
                workspace, library, active_view, ui_state, renderer_state,
                library.loaded_image_paths[remove_index]);
        }
    }

    void apply_test_engine_drop_overrides(ViewerState& viewer)
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

    void begin_developer_screenshot_request(DeveloperUiState& developer_ui,
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

    void draw_developer_windows(DeveloperUiState& developer_ui)
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
    }

    std::vector<ViewerState*>
    collect_workspace_viewers(MultiViewWorkspace& workspace)
    {
        std::vector<ViewerState*> viewers;
        viewers.reserve(workspace.view_windows.size());
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace.view_windows) {
            if (view != nullptr)
                viewers.push_back(&view->viewer);
        }
        return viewers;
    }

    std::string image_view_window_title(const ImageViewWindow& view,
                                        bool primary)
    {
        if (primary)
            return std::string(k_image_window_title);
        return Strutil::fmt::format("Image {}###imiv_image_view_{}", view.id,
                                    view.id);
    }

    void draw_image_list_window(MultiViewWorkspace& workspace,
                                ImageLibraryState& library,
                                ViewerState& active_view,
                                PlaceholderUiState& ui_state,
                                RendererState& renderer_state,
                                bool reset_layout)
    {
        static std::string s_last_logged_tooltip_path;
        bool showed_tooltip_this_frame = false;
        workspace.image_list_was_drawn = false;
        workspace.image_list_item_rects.clear();

        if (!workspace.show_image_list_window)
            return;

        if (workspace.image_list_request_focus) {
            ImGui::SetNextWindowFocus();
            workspace.image_list_request_focus = false;
        }
        if (workspace.image_list_dock_id != 0) {
            ImGui::SetNextWindowDockID(workspace.image_list_dock_id,
                                       workspace.image_list_force_dock
                                           ? ImGuiCond_Always
                                           : ImGuiCond_FirstUseEver);
        } else {
            const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
            if (main_viewport != nullptr) {
                ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 24.0f,
                                               main_viewport->WorkPos.y + 72.0f),
                                        reset_layout ? ImGuiCond_Always
                                                     : ImGuiCond_FirstUseEver);
            }
        }
        ImGui::SetNextWindowSize(ImVec2(200.0f, 420.0f),
                                 reset_layout ? ImGuiCond_Always
                                              : ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(k_image_list_window_title,
                          &workspace.show_image_list_window)) {
            workspace.image_list_was_drawn = true;
            workspace.image_list_is_docked = ImGui::IsWindowDocked();
            workspace.image_list_pos       = ImGui::GetWindowPos();
            workspace.image_list_size      = ImGui::GetWindowSize();
            ImGui::End();
            return;
        }

        workspace.image_list_was_drawn  = true;
        workspace.image_list_force_dock = !ImGui::IsWindowDocked();
        workspace.image_list_is_docked  = ImGui::IsWindowDocked();
        workspace.image_list_pos        = ImGui::GetWindowPos();
        workspace.image_list_size       = ImGui::GetWindowSize();

        ImGui::Text("Loaded images: %d",
                    static_cast<int>(library.loaded_image_paths.size()));
        ImGui::Separator();
        if (library.loaded_image_paths.empty()) {
            ImGui::TextUnformatted("No images loaded.");
            ImGui::End();
            return;
        }

        apply_test_engine_image_list_overrides(workspace, library, active_view,
                                               ui_state, renderer_state);

        enum class PendingImageListAction {
            None = 0,
            OpenInActiveView,
            OpenInNewView,
            CloseInActiveView,
            CloseInAllViews,
            RemoveFromSession
        };
        PendingImageListAction pending_action = PendingImageListAction::None;
        std::string pending_action_path;

        if (ImGui::BeginChild("##image_list_items", ImVec2(0.0f, 0.0f), false,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            for (size_t i = 0, e = library.loaded_image_paths.size(); i < e;
                 ++i) {
                const std::string& path = library.loaded_image_paths[i];
                const std::filesystem::path fs_path(path);
                const std::string filename = fs_path.filename().empty()
                                                 ? path
                                                 : fs_path.filename().string();
                const bool selected        = active_view.current_path_index
                                      == static_cast<int>(i);
                const bool active_view_image
                    = image_view_is_showing_path(active_view, path);
                const int open_count = image_list_open_view_count(workspace,
                                                                  path);
                const std::string open_badge
                    = image_list_open_view_badge(workspace, path);
                const std::string item_label = Strutil::fmt::format(
                    "{} {}. {}{}###imiv_image_list_item_{}",
                    active_view_image ? ">" : " ", static_cast<int>(i + 1),
                    filename, open_badge, static_cast<int>(i));
                const std::string test_label
                    = Strutil::fmt::format("image_list_item_{}",
                                           static_cast<int>(i));
                const std::string popup_id
                    = Strutil::fmt::format("imiv_image_list_popup_{}",
                                           static_cast<int>(i));
                const std::string close_button_id
                    = Strutil::fmt::format("x##imiv_image_list_close_{}",
                                           static_cast<int>(i));
                const float row_width   = ImGui::GetContentRegionAvail().x;
                const float close_width = ImGui::CalcTextSize("x").x
                                          + ImGui::GetStyle().FramePadding.x
                                                * 2.0f;
                const float label_width
                    = std::max(1.0f, row_width
                                         - (close_width
                                            + ImGui::GetStyle().ItemSpacing.x));
                ImGui::Selectable(item_label.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick,
                                  ImVec2(label_width, 0.0f));
                register_test_engine_item_label(test_label.c_str(), false);
                register_layout_dump_synthetic_item("selectable",
                                                    test_label.c_str());
                {
                    const ImVec2 item_min = ImGui::GetItemRectMin();
                    const ImVec2 item_max = ImGui::GetItemRectMax();
                    workspace.image_list_item_rects.emplace_back(item_min.x,
                                                                 item_min.y,
                                                                 item_max.x,
                                                                 item_max.y);
                }
                if (ImGui::IsItemHovered() && filename != path) {
                    showed_tooltip_this_frame = true;
                    if (debug_image_list_windows_enabled()
                        && s_last_logged_tooltip_path != path) {
                        print("imiv: Image List tooltip {} for '{}'\n",
                              image_list_tooltips_disabled() ? "suppressed"
                                                             : "requested",
                              path);
                        s_last_logged_tooltip_path = path;
                    }
                    if (!image_list_tooltips_disabled())
                        ImGui::SetTooltip("%s", path.c_str());
                }
                if (ImGui::IsItemClicked()) {
                    pending_action = PendingImageListAction::OpenInActiveView;
                    pending_action_path = path;
                }
                if (ImGui::IsItemHovered()
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    pending_action      = PendingImageListAction::OpenInNewView;
                    pending_action_path = path;
                }
                if (ImGui::BeginPopupContextItem(popup_id.c_str())) {
                    if (ImGui::MenuItem("Open in active view")) {
                        pending_action
                            = PendingImageListAction::OpenInActiveView;
                        pending_action_path = path;
                    }
                    if (ImGui::MenuItem("Open in new view")) {
                        pending_action = PendingImageListAction::OpenInNewView;
                        pending_action_path = path;
                    }
                    if (ImGui::MenuItem("Close in active view", nullptr, false,
                                        active_view_image)) {
                        pending_action
                            = PendingImageListAction::CloseInActiveView;
                        pending_action_path = path;
                    }
                    if (ImGui::MenuItem("Close in all views", nullptr, false,
                                        open_count > 0)) {
                        pending_action = PendingImageListAction::CloseInAllViews;
                        pending_action_path = path;
                    }
                    if (ImGui::MenuItem("Remove from session")) {
                        pending_action
                            = PendingImageListAction::RemoveFromSession;
                        pending_action_path = path;
                    }
                    ImGui::EndPopup();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(close_button_id.c_str())) {
                    pending_action = PendingImageListAction::RemoveFromSession;
                    pending_action_path = path;
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();

        switch (pending_action) {
        case PendingImageListAction::OpenInActiveView:
            if (!pending_action_path.empty()) {
                activate_image_list_path(workspace, library, active_view,
                                         ui_state, renderer_state,
                                         pending_action_path);
            }
            break;
        case PendingImageListAction::OpenInNewView:
            if (!pending_action_path.empty()) {
                open_image_list_path_in_new_view(workspace, library,
                                                 active_view, ui_state,
                                                 renderer_state,
                                                 pending_action_path);
            }
            break;
        case PendingImageListAction::CloseInActiveView:
            if (!pending_action_path.empty()) {
                close_image_list_path_in_active_view(workspace, library,
                                                     active_view, ui_state,
                                                     renderer_state,
                                                     pending_action_path);
            }
            break;
        case PendingImageListAction::CloseInAllViews:
            if (!pending_action_path.empty()) {
                close_image_list_path_in_all_views(workspace, library,
                                                   active_view, ui_state,
                                                   renderer_state,
                                                   pending_action_path);
            }
            break;
        case PendingImageListAction::RemoveFromSession:
            if (!pending_action_path.empty()) {
                remove_image_list_path_from_session(workspace, library,
                                                    active_view, ui_state,
                                                    renderer_state,
                                                    pending_action_path);
            }
            break;
        case PendingImageListAction::None: break;
        }

        if (!showed_tooltip_this_frame)
            s_last_logged_tooltip_path.clear();
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
test_engine_json_write_backend_state(FILE* f,
                                     const PlaceholderUiState& ui_state,
                                     BackendKind active_backend)
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
    test_engine_json_write_escaped(f, backend_cli_name(next_launch_backend));
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

void
test_engine_json_write_view_recipe_state(FILE* f, const ViewerState& viewer)
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
    test_engine_json_write_escaped(f,
                                   viewer.recipe.ocio_image_color_space.c_str());
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
            const std::string normalized_path = normalize_image_list_path(
                ctx->viewer->loaded_image_paths[i]);
            const std::vector<int> view_ids
                = image_list_open_view_ids(*ctx->workspace, normalized_path);
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
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar
                                            | ImGuiDockNodeFlags_NoUndocking;
    ImGui::SetNextWindowClass(&window_class);
}



void
draw_viewer_ui(MultiViewWorkspace& workspace, ImageLibraryState& library,
               PlaceholderUiState& ui_state, DeveloperUiState& developer_ui,
               const AppFonts& fonts, bool& request_exit
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
    ImageViewWindow& primary_view = ensure_primary_image_view(workspace);
    apply_test_engine_view_activation_override(workspace);
    ImageViewWindow* active_view_window = active_image_view(workspace);
    if (active_view_window == nullptr)
        active_view_window = &primary_view;
    const int frame_active_view_id = active_view_window->id;
    ViewerState& viewer            = active_view_window->viewer;
    apply_view_recipe_to_ui_state(viewer.recipe, ui_state);
    reset_layout_dump_synthetic_items();
    reset_test_engine_mouse_space();
    ViewerFrameActions actions;
    std::vector<ViewerState*> workspace_viewers = collect_workspace_viewers(
        workspace);

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
        primary_view.viewer.drag_overlay_active = true;
    apply_test_engine_ocio_overrides(ui_state);
    apply_test_engine_view_recipe_overrides(ui_state);
    set_area_sample_enabled(viewer, ui_state, ui_state.show_area_probe_window);
    update_image_list_visibility_policy(workspace, library);

    collect_viewer_shortcuts(viewer, ui_state, developer_ui, actions,
                             request_exit);
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    const bool show_test_menu = show_test_engine_windows != nullptr
                                && env_flag_is_truthy(
                                    "IMIV_IMGUI_TEST_ENGINE_SHOW_MENU");
    draw_viewer_main_menu(viewer, ui_state, library, workspace_viewers,
                          developer_ui, actions, request_exit,
                          workspace.show_image_list_window,
                          workspace.image_list_request_focus, show_test_menu,
                          show_test_engine_windows);
#else
    draw_viewer_main_menu(viewer, ui_state, library, workspace_viewers,
                          developer_ui, actions, request_exit,
                          workspace.show_image_list_window,
                          workspace.image_list_request_focus);
#endif
    begin_developer_screenshot_request(developer_ui, viewer);
    execute_viewer_frame_actions(viewer, ui_state, library, &workspace, actions
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
                                 ,
                                 window, vk_state
#endif
    );
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
    if (&primary_view.viewer != &viewer
        && !primary_view.viewer.pending_drop_paths.empty()) {
        viewer.pending_drop_paths.swap(primary_view.viewer.pending_drop_paths);
        primary_view.viewer.drag_overlay_active = false;
    }
    apply_test_engine_drop_overrides(viewer);
    process_pending_drop_paths(vk_state, viewer, library, ui_state);
    sync_workspace_library_state(workspace, viewer, library);
    (void)apply_pending_auto_subimage_action(vk_state, viewer, library,
                                             ui_state);
#endif
    clamp_placeholder_ui_state(ui_state);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    if (!viewer.image.path.empty()) {
        ui_state.subimage_index = viewer.image.subimage;
        ui_state.miplevel_index = viewer.image.miplevel;
    } else {
        ui_state.subimage_index = 0;
        ui_state.miplevel_index = 0;
    }

    const ImGuiID main_dockspace_id = begin_main_dockspace_host();
    if (actions.reset_windows_requested) {
        reset_window_layouts(workspace, ui_state, main_dockspace_id);
    }
    ensure_image_list_default_layout(workspace, main_dockspace_id);
    ImGuiWindowFlags main_window_flags = ImGuiWindowFlags_NoCollapse
                                         | ImGuiWindowFlags_NoScrollbar
                                         | ImGuiWindowFlags_NoScrollWithMouse;
    for (size_t i = 0, e = workspace.view_windows.size(); i < e; ++i) {
        ImageViewWindow& image_view = *workspace.view_windows[i];
        const bool primary          = (image_view.id == primary_view.id);
        const bool active = (image_view.id == workspace.active_view_id);
        const ImGuiID image_dock_id = workspace.image_view_dock_id != 0
                                          ? workspace.image_view_dock_id
                                          : main_dockspace_id;
        const bool force_dock       = primary ? ui_state.image_window_force_dock
                                              : image_view.force_dock;
        setup_image_window_policy(image_dock_id, force_dock);
        if (image_view.request_focus) {
            ImGui::SetNextWindowFocus();
            image_view.request_focus = false;
        }
        const std::string title = image_view_window_title(image_view, primary);
        PlaceholderUiState view_ui_state = ui_state;
        apply_view_recipe_to_ui_state(image_view.viewer.recipe, view_ui_state);
        clamp_placeholder_ui_state(view_ui_state);
        if (!image_view.viewer.image.path.empty()) {
            RendererPreviewControls preview_controls = {};
            preview_controls.exposure                = view_ui_state.exposure;
            preview_controls.gamma                   = view_ui_state.gamma;
            preview_controls.offset                  = view_ui_state.offset;
            preview_controls.color_mode              = view_ui_state.color_mode;
            preview_controls.channel     = view_ui_state.current_channel;
            preview_controls.use_ocio    = view_ui_state.use_ocio ? 1 : 0;
            preview_controls.orientation = image_view.viewer.image.orientation;
            preview_controls.linear_interpolation
                = view_ui_state.linear_interpolation ? 1 : 0;
            std::string preview_error;
            if (!renderer_update_preview_texture(
                    vk_state, image_view.viewer.texture,
                    &image_view.viewer.image, view_ui_state, preview_controls,
                    preview_error)) {
                if (!preview_error.empty())
                    image_view.viewer.last_error = preview_error;
            }
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool open = image_view.open;
        ImGui::Begin(title.c_str(), primary ? nullptr : &open,
                     main_window_flags);
        ImGui::PopStyleVar();
        image_view.open = open;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            || ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            workspace.active_view_id = image_view.id;
        }
        image_view.is_docked = ImGui::IsWindowDocked();
        if (primary) {
            view_ui_state.image_window_force_dock = !image_view.is_docked;
        } else {
            image_view.force_dock = !image_view.is_docked;
        }
        draw_image_window_contents(image_view.viewer, view_ui_state, fonts,
                                   active ? actions.pending_zoom
                                          : PendingZoomRequest(),
                                   active && actions.recenter_requested);
        ImGui::End();
        if (active) {
            ui_state.fit_image_to_window = view_ui_state.fit_image_to_window;
            ui_state.image_window_force_dock
                = view_ui_state.image_window_force_dock;
        }
    }

    for (const std::unique_ptr<ImageViewWindow>& image_view :
         workspace.view_windows) {
        if (image_view == nullptr || image_view->open
            || image_view->id == primary_view.id) {
            continue;
        }
        std::string ignored_error;
        renderer_quiesce_texture_preview_submission(vk_state,
                                                    image_view->viewer.texture,
                                                    ignored_error);
        renderer_destroy_texture(vk_state, image_view->viewer.texture);
    }
    erase_closed_image_views(workspace);
    active_view_window = active_image_view(workspace);
    if (active_view_window == nullptr)
        active_view_window = &ensure_primary_image_view(workspace);
    if (active_view_window->id != frame_active_view_id) {
        apply_view_recipe_to_ui_state(active_view_window->viewer.recipe,
                                      ui_state);
    }
    apply_test_engine_image_list_visibility_override(workspace);
    draw_image_list_window(workspace, library, active_view_window->viewer,
                           ui_state, vk_state, actions.reset_windows_requested);
    if (consume_focus_request(ui_state, k_info_window_title))
        ImGui::SetNextWindowFocus();
    draw_info_window(active_view_window->viewer, ui_state.show_info_window,
                     actions.reset_windows_requested);
    if (consume_focus_request(ui_state, k_preferences_window_title))
        ImGui::SetNextWindowFocus();
    draw_preferences_window(ui_state, ui_state.show_preferences_window,
                            renderer_active_backend(vk_state),
                            actions.reset_windows_requested);
    if (consume_focus_request(ui_state, k_preview_window_title))
        ImGui::SetNextWindowFocus();
    draw_preview_window(ui_state, ui_state.show_preview_window,
                        actions.reset_windows_requested);
    capture_view_recipe_from_ui_state(ui_state,
                                      active_view_window->viewer.recipe);
    clamp_view_recipe(active_view_window->viewer.recipe);
    apply_view_recipe_to_ui_state(active_view_window->viewer.recipe, ui_state);

    if (ui_state.show_about_window) {
        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        if (main_viewport != nullptr) {
            ImGui::SetNextWindowPos(ImVec2(main_viewport->GetCenter().x,
                                           main_viewport->GetCenter().y),
                                    ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        if (consume_focus_request(ui_state, k_about_window_title))
            ImGui::SetNextWindowFocus();
        if (ImGui::Begin(k_about_window_title, &ui_state.show_about_window,
                         ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_NoDocking)) {
            ImGui::TextUnformatted("imiv is the image viewer for OpenImageIO.");
            register_layout_dump_synthetic_item("text", "About imiv title");
            ImGui::Separator();
            ImGui::TextUnformatted(
                "(c) Copyright Contributors to the OpenImageIO project.");
            ImGui::TextUnformatted("See https://openimageio.org for details.");
            ImGui::TextUnformatted("Dear ImGui port of iv.");
            register_layout_dump_synthetic_item("text", "About imiv body");
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)
                || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                ui_state.show_about_window = false;
            }
            ImGui::SetItemDefaultFocus();
        }
        ImGui::End();
    }

    draw_developer_windows(developer_ui);
    draw_drag_drop_overlay(primary_view.viewer);
    ImGui::PopStyleVar(2);
}

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

const char*
image_window_title()
{
    return k_image_window_title;
}

}  // namespace Imiv
