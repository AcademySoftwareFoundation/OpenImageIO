// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_types.h"
#include "imiv_viewer.h"

#include <string>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

bool
load_viewer_image(VulkanState& vk_state, ViewerState& viewer,
                  PlaceholderUiState* ui_state, const std::string& path,
                  int requested_subimage, int requested_miplevel);
void
set_placeholder_status(ViewerState& viewer, const char* action);
void
save_as_dialog_action(ViewerState& viewer);
void
save_window_as_dialog_action(ViewerState& viewer);
void
save_selection_as_dialog_action(ViewerState& viewer);
void
set_sort_mode_action(ViewerState& viewer, ImageSortMode mode);
void
toggle_sort_reverse_action(ViewerState& viewer);
bool
advance_slide_show_action(VulkanState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state);
void
toggle_slide_show_action(PlaceholderUiState& ui_state, ViewerState& viewer);
void
open_image_dialog_action(VulkanState& vk_state, ViewerState& viewer,
                         PlaceholderUiState& ui_state, int requested_subimage,
                         int requested_miplevel);
void
reload_current_image_action(VulkanState& vk_state, ViewerState& viewer,
                            PlaceholderUiState& ui_state);
void
close_current_image_action(VulkanState& vk_state, ViewerState& viewer);
void
next_sibling_image_action(VulkanState& vk_state, ViewerState& viewer,
                          PlaceholderUiState& ui_state, int delta);
void
toggle_image_action(VulkanState& vk_state, ViewerState& viewer,
                    PlaceholderUiState& ui_state);
void
change_subimage_action(VulkanState& vk_state, ViewerState& viewer,
                       PlaceholderUiState& ui_state, int delta);
void
change_miplevel_action(VulkanState& vk_state, ViewerState& viewer,
                       PlaceholderUiState& ui_state, int delta);

#if defined(IMIV_BACKEND_VULKAN_GLFW)
void
set_full_screen_mode(GLFWwindow* window, ViewerState& viewer, bool enable,
                     std::string& error_message);
void
fit_window_to_image_action(GLFWwindow* window, ViewerState& viewer,
                           PlaceholderUiState& ui_state);
#endif

}  // namespace Imiv
