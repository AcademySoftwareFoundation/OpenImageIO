// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"
#include "imiv_viewer.h"

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
void
install_drag_drop(GLFWwindow* window, ViewerState& viewer);
void
uninstall_drag_drop(GLFWwindow* window);
void
process_pending_drop_paths(RendererState& renderer_state, ViewerState& viewer,
                           PlaceholderUiState& ui_state);
#endif

void
draw_drag_drop_overlay(const ViewerState& viewer);

}  // namespace Imiv
