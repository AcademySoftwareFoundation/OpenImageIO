// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_drag_drop.h"

#include "external/dnd_glfw/dnd_glfw.h"
#include "imiv_actions.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>
#endif

namespace Imiv {

#if defined(IMIV_BACKEND_VULKAN_GLFW)
namespace {

    void on_dnd_drag_enter(GLFWwindow* window, const dnd_glfw::DragEvent& event,
                           void* user_data)
    {
        (void)window;
        ViewerState* viewer = static_cast<ViewerState*>(user_data);
        if (viewer == nullptr)
            return;
        if (event.kind == dnd_glfw::PayloadKind::Files)
            viewer->drag_overlay_active = true;
    }

    void on_dnd_drag_over(GLFWwindow* window, const dnd_glfw::DragEvent& event,
                          void* user_data)
    {
        (void)window;
        (void)event;
        (void)user_data;
    }

    void on_dnd_drag_leave(GLFWwindow* window, void* user_data)
    {
        (void)window;
        ViewerState* viewer = static_cast<ViewerState*>(user_data);
        if (viewer == nullptr)
            return;
        viewer->drag_overlay_active = false;
    }

    void on_dnd_drop(GLFWwindow* window, const dnd_glfw::DropEvent& event,
                     void* user_data)
    {
        (void)window;
        ViewerState* viewer = static_cast<ViewerState*>(user_data);
        if (viewer == nullptr)
            return;
        viewer->pending_drop_paths  = event.paths;
        viewer->drag_overlay_active = false;
    }

    void on_dnd_drag_cancel(GLFWwindow* window, void* user_data)
    {
        (void)window;
        ViewerState* viewer = static_cast<ViewerState*>(user_data);
        if (viewer == nullptr)
            return;
        viewer->drag_overlay_active = false;
    }

}  // namespace

void
install_drag_drop(GLFWwindow* window, ViewerState& viewer)
{
    dnd_glfw::Callbacks callbacks = {};
    callbacks.dragEnter           = &on_dnd_drag_enter;
    callbacks.dragOver            = &on_dnd_drag_over;
    callbacks.dragLeave           = &on_dnd_drag_leave;
    callbacks.drop                = &on_dnd_drop;
    callbacks.dragCancel          = &on_dnd_drag_cancel;
    dnd_glfw::init(window, callbacks, &viewer);
}

void
uninstall_drag_drop(GLFWwindow* window)
{
    dnd_glfw::shutdown(window);
}

void
process_pending_drop_paths(VulkanState& vk_state, ViewerState& viewer,
                           PlaceholderUiState& ui_state)
{
    if (viewer.pending_drop_paths.empty())
        return;

    std::vector<std::string> drop_paths;
    drop_paths.swap(viewer.pending_drop_paths);

    int first_added_index = -1;
    append_loaded_image_paths(viewer, drop_paths, &first_added_index);

    std::string target_path;
    if (first_added_index >= 0
        && first_added_index
               < static_cast<int>(viewer.loaded_image_paths.size())) {
        target_path
            = viewer.loaded_image_paths[static_cast<size_t>(first_added_index)];
    } else {
        for (const std::string& path : drop_paths) {
            if (!set_current_loaded_image_path(viewer, path))
                continue;
            if (viewer.current_path_index < 0
                || viewer.current_path_index
                       >= static_cast<int>(viewer.loaded_image_paths.size())) {
                continue;
            }
            target_path = viewer.loaded_image_paths[static_cast<size_t>(
                viewer.current_path_index)];
            break;
        }
    }

    if (target_path.empty()) {
        viewer.status_message = "No dropped image paths were accepted";
        viewer.last_error.clear();
        return;
    }

    (void)load_viewer_image(vk_state, viewer, &ui_state, target_path,
                            ui_state.subimage_index, ui_state.miplevel_index);
}
#endif

void
draw_drag_drop_overlay(const ViewerState& viewer)
{
    if (!viewer.drag_overlay_active)
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags
        = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings
          | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking
          | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    ImGui::Begin("imiv DragDropOverlay", nullptr, flags);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

}  // namespace Imiv
