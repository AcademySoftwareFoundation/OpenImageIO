// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_platform_glfw.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include <imgui.h>
#include <imgui_impl_glfw.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    bool g_glfw_initialized = false;

    enum class GlfwPlatformPreference : uint8_t {
        Auto    = 0,
        X11     = 1,
        Wayland = 2
    };

    bool read_env_value(const char* name, std::string& out_value)
    {
        out_value.clear();
#if defined(_WIN32)
        char* value       = nullptr;
        size_t value_size = 0;
        errno_t err       = _dupenv_s(&value, &value_size, name);
        if (err != 0 || value == nullptr || value_size == 0) {
            if (value != nullptr)
                std::free(value);
            return false;
        }
        out_value.assign(value);
        std::free(value);
#else
        const char* value = std::getenv(name);
        if (value == nullptr)
            return false;
        out_value.assign(value);
#endif
        return true;
    }

    bool env_var_is_nonempty(const char* name)
    {
        std::string value;
        return read_env_value(name, value) && !value.empty();
    }

    GlfwPlatformPreference glfw_platform_preference()
    {
        std::string value;
        if (!read_env_value("IMIV_GLFW_PLATFORM", value) || value.empty())
            return GlfwPlatformPreference::Auto;
        if (Strutil::iequals(value, "x11"))
            return GlfwPlatformPreference::X11;
        if (Strutil::iequals(value, "wayland"))
            return GlfwPlatformPreference::Wayland;
        return GlfwPlatformPreference::Auto;
    }

    void glfw_error_callback(int error, const char* description)
    {
        print(stderr, "imiv: GLFW error {}: {}\n", error, description);
    }

    void configure_glfw_platform_preference(bool verbose_logging)
    {
#if !defined(_WIN32) && !defined(__APPLE__) && defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3)                                           \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
        const GlfwPlatformPreference preference = glfw_platform_preference();
        if (preference == GlfwPlatformPreference::X11) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            if (verbose_logging)
                print("imiv: GLFW platform preference = x11\n");
#    endif
            return;
        }
        if (preference == GlfwPlatformPreference::Wayland) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
            if (verbose_logging)
                print("imiv: GLFW platform preference = wayland\n");
#    endif
            return;
        }

        const bool have_x11_display     = env_var_is_nonempty("DISPLAY");
        const bool have_wayland_display = env_var_is_nonempty(
            "WAYLAND_DISPLAY");
        if (have_x11_display && have_wayland_display) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            if (verbose_logging) {
                print("imiv: GLFW auto-selected x11 for platform windows "
                      "(DISPLAY and WAYLAND_DISPLAY are both present)\n");
            }
#    endif
        }
#else
        (void)verbose_logging;
#endif
    }

    bool primary_monitor_workarea(int& x, int& y, int& w, int& h)
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr)
            return false;
#if defined(GLFW_VERSION_MAJOR)  \
    && ((GLFW_VERSION_MAJOR > 3) \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
        if (w > 0 && h > 0)
            return true;
#endif
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr)
            return false;
        x = 0;
        y = 0;
        w = mode->width;
        h = mode->height;
        return (w > 0 && h > 0);
    }

    bool centered_glfw_window_pos(GLFWwindow* window, int& out_pos_x,
                                  int& out_pos_y, int& out_window_w,
                                  int& out_window_h)
    {
        out_pos_x    = 0;
        out_pos_y    = 0;
        out_window_w = 0;
        out_window_h = 0;
        if (window == nullptr)
            return false;
        int monitor_x = 0;
        int monitor_y = 0;
        int monitor_w = 0;
        int monitor_h = 0;
        if (!primary_monitor_workarea(monitor_x, monitor_y, monitor_w,
                                      monitor_h))
            return false;
        int window_w = 0;
        int window_h = 0;
        glfwGetWindowSize(window, &window_w, &window_h);
        if (window_w <= 0 || window_h <= 0)
            return false;
        int frame_left   = 0;
        int frame_top    = 0;
        int frame_right  = 0;
        int frame_bottom = 0;
        glfwGetWindowFrameSize(window, &frame_left, &frame_top, &frame_right,
                               &frame_bottom);
        const int outer_w = window_w + frame_left + frame_right;
        const int outer_h = window_h + frame_top + frame_bottom;
        out_pos_x         = monitor_x + std::max(0, (monitor_w - outer_w) / 2)
                    + frame_left;
        out_pos_y = monitor_y + std::max(0, (monitor_h - outer_h) / 2)
                    + frame_top;
        out_window_w = window_w;
        out_window_h = window_h;
        return true;
    }

}  // namespace

bool
platform_glfw_init(bool verbose_logging, std::string& error_message)
{
    if (g_glfw_initialized) {
        error_message.clear();
        return true;
    }
    glfwSetErrorCallback(glfw_error_callback);
    configure_glfw_platform_preference(verbose_logging);
    if (glfwInit()) {
        g_glfw_initialized = true;
        error_message.clear();
        return true;
    }
    error_message = "glfwInit failed";
    return false;
}

bool
platform_glfw_is_initialized()
{
    return g_glfw_initialized;
}

void
platform_glfw_terminate()
{
    if (!g_glfw_initialized)
        return;
    g_glfw_initialized = false;
    glfwTerminate();
}

GLFWwindow*
platform_glfw_create_main_window(BackendKind backend, int width, int height,
                                 const char* title, std::string& error_message)
{
    if (backend == BackendKind::OpenGL) {
#if defined(__APPLE__)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr,
                                          nullptr);
    if (window == nullptr) {
        error_message = "failed to create GLFW window";
        return nullptr;
    }
    if (backend == BackendKind::OpenGL) {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
    }
    center_glfw_window(window);
    error_message.clear();
    return window;
}

void
platform_glfw_destroy_window(GLFWwindow* window)
{
    if (window != nullptr)
        glfwDestroyWindow(window);
}

bool
platform_glfw_supports_vulkan(std::string& error_message)
{
    if (glfwVulkanSupported()) {
        error_message.clear();
        return true;
    }
    error_message = "GLFW reports Vulkan is not supported";
    return false;
}

void
platform_glfw_collect_vulkan_instance_extensions(
    ImVector<const char*>& instance_extensions)
{
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions  = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; ++i)
        instance_extensions.push_back(glfw_extensions[i]);
}

void
platform_glfw_imgui_init(GLFWwindow* window, BackendKind backend)
{
    switch (backend) {
    case BackendKind::Vulkan: ImGui_ImplGlfw_InitForVulkan(window, true); break;
    case BackendKind::OpenGL: ImGui_ImplGlfw_InitForOpenGL(window, true); break;
    case BackendKind::Metal:
    case BackendKind::Auto: ImGui_ImplGlfw_InitForOther(window, true); break;
    }
}

void
platform_glfw_imgui_shutdown()
{
    ImGui_ImplGlfw_Shutdown();
}

void
platform_glfw_imgui_new_frame()
{
    ImGui_ImplGlfw_NewFrame();
}

void
platform_glfw_sleep(int milliseconds)
{
    ImGui_ImplGlfw_Sleep(static_cast<unsigned int>(milliseconds));
}

void
platform_glfw_poll_events()
{
    glfwPollEvents();
}

GLFWwindow*
platform_glfw_get_current_context()
{
    return glfwGetCurrentContext();
}

void
platform_glfw_make_context_current(GLFWwindow* window)
{
    glfwMakeContextCurrent(window);
}

void*
platform_glfw_get_proc_address(const char* name)
{
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

void
platform_glfw_swap_buffers(GLFWwindow* window)
{
    glfwSwapBuffers(window);
}

void
platform_glfw_show_window(GLFWwindow* window)
{
    glfwShowWindow(window);
}

bool
platform_glfw_should_close(GLFWwindow* window)
{
    return glfwWindowShouldClose(window) != 0;
}

void
platform_glfw_request_close(GLFWwindow* window)
{
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void
platform_glfw_get_framebuffer_size(GLFWwindow* window, int& width, int& height)
{
    glfwGetFramebufferSize(window, &width, &height);
}

bool
platform_glfw_is_iconified(GLFWwindow* window)
{
    return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
}

int
platform_glfw_selected_platform()
{
#if defined(GLFW_VERSION_MAJOR)  \
    && ((GLFW_VERSION_MAJOR > 3) \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
    return glfwGetPlatform();
#else
    return 0;
#endif
}

const char*
platform_glfw_name(int platform)
{
#if defined(GLFW_VERSION_MAJOR)  \
    && ((GLFW_VERSION_MAJOR > 3) \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
    switch (platform) {
#    if defined(GLFW_PLATFORM_WIN32)
    case GLFW_PLATFORM_WIN32: return "win32";
#    endif
#    if defined(GLFW_PLATFORM_COCOA)
    case GLFW_PLATFORM_COCOA: return "cocoa";
#    endif
#    if defined(GLFW_PLATFORM_WAYLAND)
    case GLFW_PLATFORM_WAYLAND: return "wayland";
#    endif
#    if defined(GLFW_PLATFORM_X11)
    case GLFW_PLATFORM_X11: return "x11";
#    endif
#    if defined(GLFW_PLATFORM_NULL)
    case GLFW_PLATFORM_NULL: return "null";
#    endif
    default: break;
    }
#else
    (void)platform;
#endif
    return "unknown";
}

void
center_glfw_window(GLFWwindow* window)
{
    int pos_x    = 0;
    int pos_y    = 0;
    int window_w = 0;
    int window_h = 0;
    if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w, window_h))
        return;
    glfwSetWindowPos(window, pos_x, pos_y);
}

void
force_center_glfw_window(GLFWwindow* window)
{
    int pos_x    = 0;
    int pos_y    = 0;
    int window_w = 0;
    int window_h = 0;
    if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w, window_h))
        return;
    glfwSetWindowMonitor(window, nullptr, pos_x, pos_y, window_w, window_h,
                         GLFW_DONT_CARE);
    glfwSetWindowPos(window, pos_x, pos_y);
}

}  // namespace Imiv
