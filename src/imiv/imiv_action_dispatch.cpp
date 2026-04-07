// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_frame_actions.h"

#include "imiv_actions.h"
#include "imiv_file_actions.h"
#include "imiv_image_library.h"
#include "imiv_workspace.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    void clear_recent_images_action(ViewerState& viewer,
                                    ImageLibraryState& library,
                                    MultiViewWorkspace* workspace)
    {
        library.recent_images.clear();
        if (workspace != nullptr) {
            sync_workspace_library_state(*workspace, library);
        } else {
            viewer.recent_images.clear();
        }
        viewer.status_message = "Cleared recent files list";
        viewer.last_error.clear();
    }

    void toggle_full_screen_action(GLFWwindow* window, ViewerState& viewer,
                                   PlaceholderUiState& ui_state)
    {
        ui_state.full_screen_mode = !ui_state.full_screen_mode;
        std::string fullscreen_error;
        set_full_screen_mode(window, viewer, ui_state.full_screen_mode,
                             fullscreen_error);
        if (!fullscreen_error.empty()) {
            viewer.last_error         = fullscreen_error;
            ui_state.full_screen_mode = viewer.fullscreen_applied;
            return;
        }

        viewer.status_message = ui_state.full_screen_mode
                                    ? "Entered full screen"
                                    : "Exited full screen";
        viewer.last_error.clear();
    }

    void delete_current_image_from_disk_action(RendererState& renderer_state,
                                               ViewerState& viewer,
                                               ImageLibraryState& library,
                                               PlaceholderUiState& ui_state)
    {
        if (viewer.image.path.empty())
            return;

        const std::string to_delete = viewer.image.path;
        close_current_image_action(renderer_state, viewer, library, ui_state);
        std::error_code ec;
        if (std::filesystem::remove(to_delete, ec)) {
            remove_loaded_image_path(library, &viewer, to_delete);
            viewer.status_message = Strutil::fmt::format("Deleted {}",
                                                         to_delete);
            viewer.last_error.clear();
            return;
        }

        viewer.last_error = ec ? Strutil::fmt::format("Delete failed: {}",
                                                      ec.message())
                               : "Delete failed";
    }

    void create_new_view_action(RendererState& renderer_state,
                                ViewerState& viewer, ImageLibraryState& library,
                                PlaceholderUiState& ui_state,
                                MultiViewWorkspace* workspace)
    {
        if (workspace == nullptr || viewer.image.path.empty())
            return;

        ImageViewWindow& new_view = append_image_view(*workspace);
        sync_viewer_library_state(new_view.viewer, library);
        new_view.viewer.recipe = viewer.recipe;
        new_view.request_focus = true;
        if (load_viewer_image(renderer_state, new_view.viewer, library,
                              &ui_state, viewer.image.path,
                              viewer.image.subimage, viewer.image.miplevel)) {
            workspace->active_view_id = new_view.id;
            sync_workspace_library_state(*workspace, library);
        }
    }

    int apply_orientation_step(int orientation, const int next_orientation[9])
    {
        return next_orientation[clamp_orientation(orientation)];
    }

    void apply_orientation_actions(ViewerState& viewer,
                                   ViewerFrameActions& actions)
    {
        if (!actions.rotate_left_requested && !actions.rotate_right_requested
            && !actions.flip_horizontal_requested
            && !actions.flip_vertical_requested) {
            return;
        }

        if (viewer.image.path.empty()) {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
        } else {
            int orientation = clamp_orientation(viewer.image.orientation);
            if (actions.rotate_left_requested) {
                static const int next_orientation[] = { 0, 8, 5, 6, 7,
                                                        4, 1, 2, 3 };
                orientation = apply_orientation_step(orientation,
                                                     next_orientation);
            }
            if (actions.rotate_right_requested) {
                static const int next_orientation[] = { 0, 6, 7, 8, 5,
                                                        2, 3, 4, 1 };
                orientation = apply_orientation_step(orientation,
                                                     next_orientation);
            }
            if (actions.flip_horizontal_requested) {
                static const int next_orientation[] = { 0, 2, 1, 4, 3,
                                                        6, 5, 8, 7 };
                orientation = apply_orientation_step(orientation,
                                                     next_orientation);
            }
            if (actions.flip_vertical_requested) {
                static const int next_orientation[] = { 0, 4, 3, 2, 1,
                                                        8, 7, 6, 5 };
                orientation = apply_orientation_step(orientation,
                                                     next_orientation);
            }
            viewer.image.orientation = clamp_orientation(orientation);
            viewer.fit_request       = true;
            viewer.status_message
                = Strutil::fmt::format("Orientation set to {}",
                                       viewer.image.orientation);
            viewer.last_error.clear();
        }

        actions.rotate_left_requested     = false;
        actions.rotate_right_requested    = false;
        actions.flip_horizontal_requested = false;
        actions.flip_vertical_requested   = false;
    }

    void advance_slide_show_if_due(RendererState& renderer_state,
                                   ViewerState& viewer,
                                   ImageLibraryState& library,
                                   PlaceholderUiState& ui_state)
    {
        if (!ui_state.slide_show_running || viewer.image.path.empty()
            || viewer.loaded_image_paths.empty()) {
            viewer.slide_last_advance_time = 0.0;
            return;
        }

        const double now = ImGui::GetTime();
        if (viewer.slide_last_advance_time <= 0.0)
            viewer.slide_last_advance_time = now;
        const double delay = std::max(1, ui_state.slide_duration_seconds);
        if (now - viewer.slide_last_advance_time >= delay) {
            (void)advance_slide_show_action(renderer_state, viewer, library,
                                            ui_state);
            viewer.slide_last_advance_time = now;
        }
    }

}  // namespace

void
execute_viewer_frame_actions(ViewerState& viewer, PlaceholderUiState& ui_state,
                             ImageLibraryState& library,
                             MultiViewWorkspace* workspace,
                             ViewerFrameActions& actions
#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
                             ,
                             GLFWwindow* window, RendererState& renderer_state
#endif
)
{
    if (actions.open_requested) {
        open_image_dialog_action(renderer_state, viewer, library, ui_state,
                                 ui_state.subimage_index,
                                 ui_state.miplevel_index);
        actions.open_requested = false;
    }
    if (actions.open_folder_requested) {
        open_folder_dialog_action(renderer_state, viewer, library, ui_state,
                                  workspace);
        actions.open_folder_requested = false;
    }
    if (!actions.recent_open_path.empty()) {
        load_viewer_image(renderer_state, viewer, library, &ui_state,
                          actions.recent_open_path, ui_state.subimage_index,
                          ui_state.miplevel_index);
        actions.recent_open_path.clear();
    }
    if (actions.clear_recent_requested) {
        clear_recent_images_action(viewer, library, workspace);
        actions.clear_recent_requested = false;
    }
    if (actions.reload_requested) {
        reload_current_image_action(renderer_state, viewer, library, ui_state);
        actions.reload_requested = false;
    }
    if (actions.close_requested) {
        close_current_image_action(renderer_state, viewer, library, ui_state);
        actions.close_requested = false;
    }
    if (actions.prev_requested) {
        next_sibling_image_action(renderer_state, viewer, library, ui_state,
                                  -1);
        actions.prev_requested = false;
    }
    if (actions.next_requested) {
        next_sibling_image_action(renderer_state, viewer, library, ui_state, 1);
        actions.next_requested = false;
    }
    if (actions.toggle_requested) {
        toggle_image_action(renderer_state, viewer, library, ui_state);
        actions.toggle_requested = false;
    }
    if (actions.prev_subimage_requested) {
        change_subimage_action(renderer_state, viewer, library, ui_state, -1);
        actions.prev_subimage_requested = false;
    }
    if (actions.next_subimage_requested) {
        change_subimage_action(renderer_state, viewer, library, ui_state, 1);
        actions.next_subimage_requested = false;
    }
    if (actions.prev_mip_requested) {
        change_miplevel_action(renderer_state, viewer, library, ui_state, -1);
        actions.prev_mip_requested = false;
    }
    if (actions.next_mip_requested) {
        change_miplevel_action(renderer_state, viewer, library, ui_state, 1);
        actions.next_mip_requested = false;
    }
    if (actions.save_as_requested) {
        save_as_dialog_action(viewer);
        actions.save_as_requested = false;
    }
    if (actions.save_window_as_requested) {
        save_window_as_dialog_action(viewer, ui_state);
        actions.save_window_as_requested = false;
    }
    if (actions.save_selection_as_requested) {
        save_selection_as_dialog_action(viewer);
        actions.save_selection_as_requested = false;
    }
    if (actions.export_selection_as_requested) {
        export_selection_as_dialog_action(viewer, ui_state);
        actions.export_selection_as_requested = false;
    }
    if (actions.select_all_requested) {
        select_all_image_action(viewer, ui_state);
        actions.select_all_requested = false;
    }
    if (actions.deselect_selection_requested) {
        deselect_selection_action(viewer, ui_state);
        actions.deselect_selection_requested = false;
    }
    if (actions.fit_window_to_image_requested) {
        fit_window_to_image_action(window, viewer, ui_state);
        actions.fit_window_to_image_requested = false;
    }
    if (actions.full_screen_toggle_requested) {
        toggle_full_screen_action(window, viewer, ui_state);
        actions.full_screen_toggle_requested = false;
    }
    if (actions.delete_from_disk_requested) {
        delete_current_image_from_disk_action(renderer_state, viewer, library,
                                              ui_state);
        actions.delete_from_disk_requested = false;
    }
    if (actions.new_view_requested) {
        create_new_view_action(renderer_state, viewer, library, ui_state,
                               workspace);
    }
    actions.new_view_requested = false;

    apply_orientation_actions(viewer, actions);
    advance_slide_show_if_due(renderer_state, viewer, library, ui_state);

    if (workspace != nullptr)
        sync_workspace_library_state(*workspace, library);
}

}  // namespace Imiv
