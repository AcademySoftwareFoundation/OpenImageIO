// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_workspace_ui.h"

#include "imiv_actions.h"
#include "imiv_parse.h"
#include "imiv_test_engine.h"
#include "imiv_ui_metrics.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_image_window_title      = "Image";
    constexpr const char* k_image_list_window_title = "Image List";

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
    image_list_open_view_ids_for_path(const MultiViewWorkspace& workspace,
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
            = image_list_open_view_ids_for_path(workspace, normalized_path);
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
        sync_workspace_library_state(workspace, library);
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
        sync_workspace_library_state(workspace, library);
        return true;
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
        sync_workspace_library_state(workspace, library);
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
        sync_workspace_library_state(workspace, library);
        update_image_list_visibility_policy(workspace, library);
        if (was_image_list_visible && !library.loaded_image_paths.empty()) {
            workspace.show_image_list_window = true;
        }
        active_view.status_message
            = Strutil::fmt::format("Removed {} from session", path);
        active_view.last_error.clear();
        return true;
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

}  // namespace

const char*
image_window_title()
{
    return k_image_window_title;
}



void
update_image_list_visibility_policy(MultiViewWorkspace& workspace,
                                    const ImageLibraryState& library)
{
    const int image_count = static_cast<int>(library.loaded_image_paths.size());
    if (image_count <= 0) {
        workspace.show_image_list_window   = false;
        workspace.image_list_force_dock    = false;
        workspace.last_library_image_count = image_count;
        return;
    }

    if (image_count > 1 && image_count != workspace.last_library_image_count) {
        workspace.show_image_list_window = true;
        workspace.image_list_force_dock  = true;
    }
    workspace.last_library_image_count = image_count;
}



void
ensure_image_list_default_layout(MultiViewWorkspace& workspace,
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
    if (dockspace_node == nullptr || dockspace_node->Size.x <= 0.0f)
        return;

    const float ratio = std::clamp(UiMetrics::ImageList::kDockTargetWidth
                                       / dockspace_node->Size.x,
                                   UiMetrics::ImageList::kDockMinRatio,
                                   UiMetrics::ImageList::kDockMaxRatio);
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
    ImGui::DockBuilderDockWindow(k_image_list_window_title, image_list_dock_id);
    ImGui::DockBuilderFinish(dockspace_id);
    workspace.image_list_layout_initialized = true;
}



void
reset_window_layouts(MultiViewWorkspace& workspace,
                     PlaceholderUiState& ui_state, ImGuiID dockspace_id)
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
    workspace.image_list_force_dock         = workspace.show_image_list_window;
    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view != nullptr) {
            view->force_dock    = true;
            view->request_focus = (view->id == workspace.active_view_id);
        }
    }
    ImGui::GetIO().WantSaveIniSettings = true;
}



void
apply_test_engine_image_list_visibility_override(MultiViewWorkspace& workspace)
{
    const int apply_frame
        = env_int_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_APPLY_FRAME", -1);
    if (apply_frame < 0 || ImGui::GetFrameCount() != apply_frame)
        return;

    std::string show_image_list_value;
    if (read_env_value("IMIV_IMGUI_TEST_ENGINE_IMAGE_LIST_VISIBLE",
                       show_image_list_value)) {
        bool visible = false;
        if (parse_bool_string(show_image_list_value, visible)) {
            workspace.show_image_list_window = visible;
            if (visible)
                workspace.image_list_force_dock = true;
        }
    }
}



std::vector<int>
image_list_open_view_ids(const MultiViewWorkspace& workspace,
                         const std::string& path)
{
    return image_list_open_view_ids_for_path(workspace,
                                             normalize_image_list_path(path));
}



void
draw_image_list_window(MultiViewWorkspace& workspace,
                       ImageLibraryState& library, ViewerState& active_view,
                       PlaceholderUiState& ui_state,
                       RendererState& renderer_state, bool reset_layout)
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
            ImGui::SetNextWindowPos(
                ImVec2(main_viewport->WorkPos.x
                           + UiMetrics::ImageList::kFloatingOffset.x,
                       main_viewport->WorkPos.y
                           + UiMetrics::ImageList::kFloatingOffset.y),
                reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
        }
    }
    ImGui::SetNextWindowSize(UiMetrics::ImageList::kDefaultWindowSize,
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
        for (size_t i = 0, e = library.loaded_image_paths.size(); i < e; ++i) {
            const std::string& path           = library.loaded_image_paths[i];
            const std::string normalized_path = normalize_image_list_path(path);
            const std::filesystem::path fs_path(path);
            const std::string filename = fs_path.filename().empty()
                                             ? path
                                             : fs_path.filename().string();
            const bool selected        = active_view.current_path_index
                                  == static_cast<int>(i);
            const bool active_view_image
                = image_view_is_showing_path(active_view, normalized_path);
            const int open_count = image_list_open_view_count(workspace,
                                                              normalized_path);
            const std::string open_badge
                = image_list_open_view_badge(workspace, normalized_path);
            const std::string item_label
                = Strutil::fmt::format("{} {}. {}{}###imiv_image_list_item_{}",
                                       active_view_image ? ">" : " ",
                                       static_cast<int>(i + 1), filename,
                                       open_badge, static_cast<int>(i));
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
                                      + ImGui::GetStyle().FramePadding.x * 2.0f;
            const float label_width = std::max(
                1.0f,
                row_width - (close_width + ImGui::GetStyle().ItemSpacing.x));
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
                pending_action      = PendingImageListAction::OpenInActiveView;
                pending_action_path = path;
            }
            if (ImGui::IsItemHovered()
                && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                pending_action      = PendingImageListAction::OpenInNewView;
                pending_action_path = path;
            }
            if (ImGui::BeginPopupContextItem(popup_id.c_str())) {
                if (ImGui::MenuItem("Open in active view")) {
                    pending_action = PendingImageListAction::OpenInActiveView;
                    pending_action_path = path;
                }
                if (ImGui::MenuItem("Open in new view")) {
                    pending_action      = PendingImageListAction::OpenInNewView;
                    pending_action_path = path;
                }
                if (ImGui::MenuItem("Close in active view", nullptr, false,
                                    active_view_image)) {
                    pending_action = PendingImageListAction::CloseInActiveView;
                    pending_action_path = path;
                }
                if (ImGui::MenuItem("Close in all views", nullptr, false,
                                    open_count > 0)) {
                    pending_action = PendingImageListAction::CloseInAllViews;
                    pending_action_path = path;
                }
                if (ImGui::MenuItem("Remove from session")) {
                    pending_action = PendingImageListAction::RemoveFromSession;
                    pending_action_path = path;
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(close_button_id.c_str())) {
                pending_action      = PendingImageListAction::RemoveFromSession;
                pending_action_path = path;
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();

    switch (pending_action) {
    case PendingImageListAction::OpenInActiveView:
        if (!pending_action_path.empty()) {
            activate_image_list_path(workspace, library, active_view, ui_state,
                                     renderer_state, pending_action_path);
        }
        break;
    case PendingImageListAction::OpenInNewView:
        if (!pending_action_path.empty()) {
            open_image_list_path_in_new_view(workspace, library, active_view,
                                             ui_state, renderer_state,
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
            close_image_list_path_in_all_views(workspace, library, active_view,
                                               ui_state, renderer_state,
                                               pending_action_path);
        }
        break;
    case PendingImageListAction::RemoveFromSession:
        if (!pending_action_path.empty()) {
            remove_image_list_path_from_session(workspace, library, active_view,
                                                ui_state, renderer_state,
                                                pending_action_path);
        }
        break;
    case PendingImageListAction::None: break;
    }

    if (!showed_tooltip_this_frame)
        s_last_logged_tooltip_path.clear();
}

}  // namespace Imiv
