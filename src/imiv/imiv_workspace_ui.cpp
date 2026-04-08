// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_workspace_ui.h"

#include "imiv_ui_metrics.h"

#include <algorithm>
#include <string>

#include <imgui_internal.h>

namespace Imiv {

namespace {

    constexpr const char* k_image_window_title      = "Image";
    constexpr const char* k_image_list_window_title = "Image List";

}  // namespace

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



}  // namespace Imiv
