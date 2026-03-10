// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_ui.h"

#include <filesystem>
#include <string>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
struct GLFWwindow;
#endif

namespace Imiv {

#if defined(IMGUI_ENABLE_TEST_ENGINE)
struct ViewerStateJsonWriteContext {
    const ViewerState* viewer          = nullptr;
    const PlaceholderUiState* ui_state = nullptr;
};

bool
write_test_engine_viewer_state_json(const std::filesystem::path& out_path,
                                    void* user_data,
                                    std::string& error_message);
#endif

const char*
image_window_title();

#if defined(IMIV_BACKEND_VULKAN_GLFW)
void
center_glfw_window(GLFWwindow* window);
void
force_center_glfw_window(GLFWwindow* window);
void
set_glfw_error_callback();
#endif

void
draw_viewer_ui(ViewerState& viewer, PlaceholderUiState& ui_state,
               const AppFonts& fonts, bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
               ,
               bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW)
               ,
               GLFWwindow* window, VulkanState& vk_state
#endif
);

}  // namespace Imiv
