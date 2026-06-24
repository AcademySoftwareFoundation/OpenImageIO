// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_renderer.h"
#include "imiv_ui.h"

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

struct DeveloperUiState;

inline constexpr const char* k_image_window_title = "Image";

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
               GLFWwindow* window, RendererState& renderer_state
#endif
);

}  // namespace Imiv
