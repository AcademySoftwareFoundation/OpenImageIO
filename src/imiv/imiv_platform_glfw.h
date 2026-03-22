// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_backend.h"

#include <imgui.h>

#include <string>

struct GLFWwindow;

namespace Imiv {

bool
platform_glfw_init(bool verbose_logging, std::string& error_message);
bool
platform_glfw_is_initialized();
void
platform_glfw_terminate();

GLFWwindow*
platform_glfw_create_main_window(BackendKind backend, int width, int height,
                                 const char* title,
                                 std::string& error_message);
void
platform_glfw_destroy_window(GLFWwindow* window);

bool
platform_glfw_supports_vulkan(std::string& error_message);
void
platform_glfw_collect_vulkan_instance_extensions(
    ImVector<const char*>& instance_extensions);

void
platform_glfw_imgui_init(GLFWwindow* window, BackendKind backend);
void
platform_glfw_imgui_shutdown();
void
platform_glfw_imgui_new_frame();
void
platform_glfw_sleep(int milliseconds);
void
platform_glfw_poll_events();
GLFWwindow*
platform_glfw_get_current_context();
void
platform_glfw_make_context_current(GLFWwindow* window);
void*
platform_glfw_get_proc_address(const char* name);
void
platform_glfw_swap_buffers(GLFWwindow* window);
void
platform_glfw_show_window(GLFWwindow* window);
bool
platform_glfw_should_close(GLFWwindow* window);
void
platform_glfw_request_close(GLFWwindow* window);
void
platform_glfw_get_framebuffer_size(GLFWwindow* window, int& width, int& height);
bool
platform_glfw_is_iconified(GLFWwindow* window);

int
platform_glfw_selected_platform();
const char*
platform_glfw_name(int platform);

void
center_glfw_window(GLFWwindow* window);
void
force_center_glfw_window(GLFWwindow* window);

}  // namespace Imiv
