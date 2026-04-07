// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_frame.h"

#include "imiv_actions.h"
#include "imiv_backend.h"
#include "imiv_developer_tools.h"
#include "imiv_drag_drop.h"
#include "imiv_frame_actions.h"
#include "imiv_image_view.h"
#include "imiv_menu.h"
#include "imiv_parse.h"
#include "imiv_test_engine.h"
#include "imiv_ui.h"
#include "imiv_ui_metrics.h"
#include "imiv_workspace.h"
#include "imiv_workspace_ui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_dockspace_host_title = "imiv DockSpace";

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
            return std::string(image_window_title());
        return Strutil::fmt::format("Image {}###imiv_image_view_{}", view.id,
                                    view.id);
    }

}  // namespace



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
    sync_workspace_library_state(workspace, library);
    (void)apply_pending_auto_subimage_action(vk_state, viewer, library,
                                             ui_state);
#endif
    clamp_placeholder_ui_state(ui_state);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        UiMetrics::kAppFramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UiMetrics::kAppItemSpacing);

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
            PreviewControls preview_controls = {};
            preview_controls.exposure        = view_ui_state.exposure;
            preview_controls.gamma           = view_ui_state.gamma;
            preview_controls.offset          = view_ui_state.offset;
            preview_controls.color_mode      = view_ui_state.color_mode;
            preview_controls.channel         = view_ui_state.current_channel;
            preview_controls.use_ocio        = view_ui_state.use_ocio ? 1 : 0;
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

}  // namespace Imiv
