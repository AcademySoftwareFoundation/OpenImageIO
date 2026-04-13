// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"
#include "imiv_viewer.h"

#include <string>

namespace Imiv {

void
save_as_dialog_action(ViewerState& viewer);
void
save_window_as_dialog_action(ViewerState& viewer,
                             const PlaceholderUiState& ui_state);
void
save_selection_as_dialog_action(ViewerState& viewer);
void
export_selection_as_dialog_action(ViewerState& viewer,
                                  const PlaceholderUiState& ui_state);
void
open_image_dialog_action(RendererState& renderer_state, ViewerState& viewer,
                         ImageLibraryState& library,
                         PlaceholderUiState& ui_state, int requested_subimage,
                         int requested_miplevel);
void
open_folder_dialog_action(RendererState& renderer_state, ViewerState& viewer,
                          ImageLibraryState& library,
                          PlaceholderUiState& ui_state,
                          MultiViewWorkspace* workspace);
bool
capture_main_viewport_screenshot_action(RendererState& renderer_state,
                                        ViewerState& viewer,
                                        std::string& out_path);

}  // namespace Imiv
