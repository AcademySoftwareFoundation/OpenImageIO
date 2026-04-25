// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer.h"

#include "imiv_build_config.h"
#include "imiv_platform_glfw.h"
#include "imiv_renderer_backend.h"
#include "imiv_viewer.h"

#include <array>
#include <string_view>

#include <OpenImageIO/strutil.h>

namespace Imiv {

namespace {

    const RendererBackendVTable* renderer_backend_vtable(BackendKind kind)
    {
        switch (kind) {
#if IMIV_WITH_VULKAN
        case BackendKind::Vulkan: return renderer_backend_vulkan_vtable();
#else
        case BackendKind::Vulkan: break;
#endif
#if IMIV_WITH_METAL
        case BackendKind::Metal: return renderer_backend_metal_vtable();
#else
        case BackendKind::Metal: break;
#endif
#if IMIV_WITH_OPENGL
        case BackendKind::OpenGL: return renderer_backend_opengl_vtable();
#else
        case BackendKind::OpenGL: break;
#endif
        case BackendKind::Auto: break;
        }
        return nullptr;
    }

    const RendererBackendVTable*
    renderer_dispatch_vtable(const RendererState& renderer_state)
    {
        if (renderer_state.vtable != nullptr)
            return renderer_state.vtable;
        return renderer_backend_vtable(renderer_state.active_backend);
    }

    const RendererBackendVTable*
    texture_dispatch_vtable(const RendererTexture& texture)
    {
        if (texture.vtable != nullptr)
            return texture.vtable;
        return renderer_backend_vtable(texture.backend_kind);
    }

    BackendInfo make_backend_info(BackendKind kind, const char* cli_name,
                                  const char* display_name)
    {
        BackendInfo info;
        info.kind         = kind;
        info.cli_name     = cli_name;
        info.display_name = display_name;
        switch (kind) {
        case BackendKind::Vulkan: info.compiled = IMIV_WITH_VULKAN != 0; break;
        case BackendKind::Metal: info.compiled = IMIV_WITH_METAL != 0; break;
        case BackendKind::OpenGL: info.compiled = IMIV_WITH_OPENGL != 0; break;
        case BackendKind::Auto: info.compiled = false; break;
        }
        info.active_build     = (kind == active_build_backend_kind());
        info.platform_default = (kind == platform_default_backend_kind());
        return info;
    }

    size_t backend_info_index(BackendKind kind)
    {
        switch (kind) {
        case BackendKind::Vulkan: return 0;
        case BackendKind::Metal: return 1;
        case BackendKind::OpenGL: return 2;
        case BackendKind::Auto: break;
        }
        return compiled_backend_info().size();
    }

    template<class Predicate>
    BackendKind resolve_backend_with_predicate(BackendKind requested_kind,
                                               Predicate predicate)
    {
        if (requested_kind != BackendKind::Auto && predicate(requested_kind))
            return requested_kind;

        const BackendKind build_default = active_build_backend_kind();
        if (build_default != BackendKind::Auto && predicate(build_default))
            return build_default;

        const BackendKind platform_default = platform_default_backend_kind();
        if (platform_default != BackendKind::Auto
            && predicate(platform_default))
            return platform_default;

        if (predicate(BackendKind::Vulkan))
            return BackendKind::Vulkan;
        if (predicate(BackendKind::Metal))
            return BackendKind::Metal;
        if (predicate(BackendKind::OpenGL))
            return BackendKind::OpenGL;

        return BackendKind::Auto;
    }

    std::array<BackendRuntimeInfo, 3>& mutable_runtime_backend_info()
    {
        static std::array<BackendRuntimeInfo, 3> info;
        return info;
    }

    bool& runtime_backend_info_is_valid_flag()
    {
        static bool valid = false;
        return valid;
    }

    void reset_runtime_backend_info()
    {
        std::array<BackendRuntimeInfo, 3>& info = mutable_runtime_backend_info();
        const std::array<BackendInfo, 3>& compiled = compiled_backend_info();
        for (size_t i = 0; i < info.size(); ++i) {
            info[i].build_info = compiled[i];
            info[i].available  = compiled[i].compiled;
            info[i].probed     = false;
            info[i].unavailable_reason.clear();
        }
    }

}  // namespace

BackendKind
sanitize_backend_kind(int value)
{
    switch (static_cast<BackendKind>(value)) {
    case BackendKind::Auto:
    case BackendKind::Vulkan:
    case BackendKind::Metal:
    case BackendKind::OpenGL: return static_cast<BackendKind>(value);
    }
    return BackendKind::Auto;
}

DisplayFormatPreference
sanitize_display_format_preference(int value)
{
    switch (static_cast<DisplayFormatPreference>(value)) {
    case DisplayFormatPreference::Auto:
    case DisplayFormatPreference::Rgba8:
    case DisplayFormatPreference::Rgb10A2:
    case DisplayFormatPreference::Hdr:
        return static_cast<DisplayFormatPreference>(value);
    }
    return DisplayFormatPreference::Auto;
}

bool
parse_backend_kind(std::string_view value, BackendKind& out_kind)
{
    const std::string normalized = OIIO::Strutil::lower(
        std::string(OIIO::Strutil::strip(value)));
    if (normalized.empty() || normalized == "auto") {
        out_kind = BackendKind::Auto;
        return true;
    }
    if (normalized == "vulkan") {
        out_kind = BackendKind::Vulkan;
        return true;
    }
    if (normalized == "metal") {
        out_kind = BackendKind::Metal;
        return true;
    }
    if (normalized == "opengl" || normalized == "gl") {
        out_kind = BackendKind::OpenGL;
        return true;
    }
    return false;
}

bool
parse_display_format_preference(std::string_view value,
                                DisplayFormatPreference& out_format)
{
    const std::string normalized = OIIO::Strutil::lower(
        std::string(OIIO::Strutil::strip(value)));
    if (normalized.empty() || normalized == "auto") {
        out_format = DisplayFormatPreference::Auto;
        return true;
    }
    if (normalized == "rgba8" || normalized == "8bit" || normalized == "8-bit"
        || normalized == "sdr8") {
        out_format = DisplayFormatPreference::Rgba8;
        return true;
    }
    if (normalized == "rgb10a2" || normalized == "10bit"
        || normalized == "10-bit" || normalized == "sdr10") {
        out_format = DisplayFormatPreference::Rgb10A2;
        return true;
    }
    if (normalized == "hdr" || normalized == "edr") {
        out_format = DisplayFormatPreference::Hdr;
        return true;
    }
    return false;
}

const char*
backend_cli_name(BackendKind kind)
{
    switch (kind) {
    case BackendKind::Auto: return "auto";
    case BackendKind::Vulkan: return "vulkan";
    case BackendKind::Metal: return "metal";
    case BackendKind::OpenGL: return "opengl";
    }
    return "auto";
}

const char*
display_format_cli_name(DisplayFormatPreference format)
{
    switch (format) {
    case DisplayFormatPreference::Auto: return "auto";
    case DisplayFormatPreference::Rgba8: return "rgba8";
    case DisplayFormatPreference::Rgb10A2: return "rgb10a2";
    case DisplayFormatPreference::Hdr: return "hdr";
    }
    return "auto";
}

const char*
display_format_display_name(DisplayFormatPreference format)
{
    switch (format) {
    case DisplayFormatPreference::Auto: return "Auto";
    case DisplayFormatPreference::Rgba8: return "RGBA8";
    case DisplayFormatPreference::Rgb10A2: return "RGB10A2";
    case DisplayFormatPreference::Hdr: return "HDR/EDR";
    }
    return "Auto";
}

const char*
backend_display_name(BackendKind kind)
{
    switch (kind) {
    case BackendKind::Auto: return "Auto";
    case BackendKind::Vulkan: return "Vulkan";
    case BackendKind::Metal: return "Metal";
    case BackendKind::OpenGL: return "OpenGL";
    }
    return "Auto";
}

const char*
backend_runtime_name(BackendKind kind)
{
    switch (kind) {
    case BackendKind::Vulkan: return "glfw+vulkan";
    case BackendKind::Metal: return "glfw+metal";
    case BackendKind::OpenGL: return "glfw+opengl";
    case BackendKind::Auto: return "auto";
    }
    return "auto";
}

BackendKind
active_build_backend_kind()
{
    return sanitize_backend_kind(IMIV_BUILD_DEFAULT_BACKEND_KIND);
}

BackendKind
platform_default_backend_kind()
{
#if defined(__APPLE__)
    if (IMIV_WITH_METAL)
        return BackendKind::Metal;
    if (IMIV_WITH_VULKAN)
        return BackendKind::Vulkan;
    if (IMIV_WITH_OPENGL)
        return BackendKind::OpenGL;
#else
    if (IMIV_WITH_VULKAN)
        return BackendKind::Vulkan;
    if (IMIV_WITH_OPENGL)
        return BackendKind::OpenGL;
    if (IMIV_WITH_METAL)
        return BackendKind::Metal;
#endif
    return BackendKind::Auto;
}

BackendKind
resolve_backend_request(BackendKind requested_kind)
{
    return resolve_backend_with_predicate(requested_kind,
                                          backend_kind_is_available);
}

bool
refresh_runtime_backend_info(bool verbose_logging, std::string& error_message)
{
    error_message.clear();
    reset_runtime_backend_info();

    bool initialized_here = false;
    if (!platform_glfw_is_initialized()) {
        if (!platform_glfw_init(verbose_logging, error_message)) {
            std::array<BackendRuntimeInfo, 3>& info
                = mutable_runtime_backend_info();
            for (BackendRuntimeInfo& runtime_info : info) {
                if (!runtime_info.build_info.compiled)
                    continue;
                runtime_info.available          = false;
                runtime_info.probed             = true;
                runtime_info.unavailable_reason = error_message;
            }
            runtime_backend_info_is_valid_flag() = true;
            return false;
        }
        initialized_here = true;
    }

    std::array<BackendRuntimeInfo, 3>& info = mutable_runtime_backend_info();
    for (BackendRuntimeInfo& runtime_info : info) {
        if (!runtime_info.build_info.compiled)
            continue;
        std::string probe_error;
        runtime_info.available = renderer_probe_backend_runtime_support(
            runtime_info.build_info.kind, probe_error);
        runtime_info.probed = true;
        if (!runtime_info.available) {
            runtime_info.unavailable_reason = probe_error.empty()
                                                  ? "backend is unavailable"
                                                  : probe_error;
        }
    }

    runtime_backend_info_is_valid_flag() = true;
    if (initialized_here)
        platform_glfw_terminate();
    return true;
}

void
clear_runtime_backend_info()
{
    reset_runtime_backend_info();
    runtime_backend_info_is_valid_flag() = false;
}

bool
runtime_backend_info_valid()
{
    return runtime_backend_info_is_valid_flag();
}

bool
backend_kind_is_available(BackendKind kind)
{
    if (!backend_kind_is_compiled(kind))
        return false;
    if (!runtime_backend_info_valid())
        return true;
    const size_t index = backend_info_index(kind);
    if (index >= mutable_runtime_backend_info().size())
        return false;
    return mutable_runtime_backend_info()[index].available;
}

std::string_view
backend_unavailable_reason(BackendKind kind)
{
    if (!runtime_backend_info_valid())
        return {};
    const size_t index = backend_info_index(kind);
    if (index >= mutable_runtime_backend_info().size())
        return {};
    return mutable_runtime_backend_info()[index].unavailable_reason;
}

bool
backend_kind_is_compiled(BackendKind kind)
{
    switch (kind) {
    case BackendKind::Auto: return false;
    case BackendKind::Vulkan: return IMIV_WITH_VULKAN != 0;
    case BackendKind::Metal: return IMIV_WITH_METAL != 0;
    case BackendKind::OpenGL: return IMIV_WITH_OPENGL != 0;
    }
    return false;
}

const std::array<BackendInfo, 3>&
compiled_backend_info()
{
    static const std::array<BackendInfo, 3> info = {
        make_backend_info(BackendKind::Vulkan, "vulkan", "Vulkan"),
        make_backend_info(BackendKind::Metal, "metal", "Metal"),
        make_backend_info(BackendKind::OpenGL, "opengl", "OpenGL"),
    };
    return info;
}

const std::array<BackendRuntimeInfo, 3>&
runtime_backend_info()
{
    if (!runtime_backend_info_valid())
        reset_runtime_backend_info();
    return mutable_runtime_backend_info();
}

size_t
compiled_backend_count()
{
    size_t count = 0;
    for (const BackendInfo& info : compiled_backend_info()) {
        if (info.compiled)
            ++count;
    }
    return count;
}

void
renderer_select_backend(RendererState& renderer_state, BackendKind backend)
{
    renderer_state.active_backend = backend;
    renderer_state.vtable         = renderer_backend_vtable(backend);
}

BackendKind
renderer_active_backend(const RendererState& renderer_state)
{
    return renderer_state.active_backend;
}

bool
renderer_probe_backend_runtime_support(BackendKind backend,
                                       std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_backend_vtable(backend);
    if (vtable == nullptr || vtable->probe_runtime_support == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->probe_runtime_support(error_message);
}

bool
renderer_texture_is_loading(const RendererTexture& texture)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr || vtable->texture_is_loading == nullptr)
        return false;
    return vtable->texture_is_loading(texture);
}

void
renderer_get_viewer_texture_refs(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ImTextureRef& main_texture_ref,
                                 bool& has_main_texture,
                                 ImTextureRef& closeup_texture_ref,
                                 bool& has_closeup_texture)
{
    main_texture_ref                    = ImTextureRef();
    closeup_texture_ref                 = ImTextureRef();
    has_main_texture                    = false;
    has_closeup_texture                 = false;
    const RendererBackendVTable* vtable = texture_dispatch_vtable(
        viewer.texture);
    if (vtable == nullptr || vtable->get_viewer_texture_refs == nullptr)
        return;
    vtable->get_viewer_texture_refs(viewer, ui_state, main_texture_ref,
                                    has_main_texture, closeup_texture_ref,
                                    has_closeup_texture);
}

bool
renderer_create_texture(RendererState& renderer_state, const LoadedImage& image,
                        RendererTexture& texture, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->create_texture == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    const bool ok = vtable->create_texture(renderer_state, image, texture,
                                           error_message);
    if (ok) {
        texture.backend_kind = renderer_state.active_backend;
        texture.vtable       = vtable;
    }
    return ok;
}

void
renderer_destroy_texture(RendererState& renderer_state,
                         RendererTexture& texture)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable != nullptr && vtable->destroy_texture != nullptr)
        vtable->destroy_texture(renderer_state, texture);
    texture.vtable              = nullptr;
    texture.backend_kind        = BackendKind::Auto;
    texture.backend             = nullptr;
    texture.preview_initialized = false;
}

bool
renderer_update_preview_texture(RendererState& renderer_state,
                                RendererTexture& texture,
                                const LoadedImage* image,
                                const PlaceholderUiState& ui_state,
                                const PreviewControls& controls,
                                std::string& error_message)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr || vtable->update_preview_texture == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->update_preview_texture(renderer_state, texture, image,
                                          ui_state, controls, error_message);
}

bool
renderer_quiesce_texture_preview_submission(RendererState& renderer_state,
                                            RendererTexture& texture,
                                            std::string& error_message)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr
        || vtable->quiesce_texture_preview_submission == nullptr) {
        error_message.clear();
        return true;
    }
    return vtable->quiesce_texture_preview_submission(renderer_state, texture,
                                                      error_message);
}

bool
renderer_setup_instance(RendererState& renderer_state,
                        ImVector<const char*>& instance_extensions,
                        std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_instance == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_instance(renderer_state, instance_extensions,
                                  error_message);
}

bool
renderer_setup_device(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_device == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_device(renderer_state, error_message);
}

bool
renderer_setup_window(RendererState& renderer_state, int width, int height,
                      std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_window == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_window(renderer_state, width, height, error_message);
}

bool
renderer_create_surface(RendererState& renderer_state, GLFWwindow* window,
                        std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->create_surface == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->create_surface(renderer_state, window, error_message);
}

void
renderer_destroy_surface(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->destroy_surface != nullptr)
        vtable->destroy_surface(renderer_state);
}

void
renderer_cleanup_window(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->cleanup_window != nullptr)
        vtable->cleanup_window(renderer_state);
}

void
renderer_cleanup(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->cleanup != nullptr)
        vtable->cleanup(renderer_state);
}

bool
renderer_wait_idle(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->wait_idle == nullptr) {
        error_message.clear();
        return true;
    }
    return vtable->wait_idle(renderer_state, error_message);
}

bool
renderer_imgui_init(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->imgui_init == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->imgui_init(renderer_state, error_message);
}

void
renderer_imgui_shutdown(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->imgui_shutdown != nullptr)
        vtable->imgui_shutdown();
}

void
renderer_imgui_new_frame(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->imgui_new_frame != nullptr)
        vtable->imgui_new_frame(renderer_state);
}

bool
renderer_needs_main_window_resize(RendererState& renderer_state, int width,
                                  int height)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->needs_main_window_resize == nullptr)
        return false;
    return vtable->needs_main_window_resize(renderer_state, width, height);
}

void
renderer_resize_main_window(RendererState& renderer_state, int width,
                            int height)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->resize_main_window != nullptr)
        vtable->resize_main_window(renderer_state, width, height);
}

void
renderer_set_main_clear_color(RendererState& renderer_state, float r, float g,
                              float b, float a)
{
    renderer_state.clear_color[0]       = r;
    renderer_state.clear_color[1]       = g;
    renderer_state.clear_color[2]       = b;
    renderer_state.clear_color[3]       = a;
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->set_main_clear_color != nullptr)
        vtable->set_main_clear_color(renderer_state, r, g, b, a);
}

void
renderer_prepare_platform_windows(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->prepare_platform_windows != nullptr)
        vtable->prepare_platform_windows(renderer_state);
}

void
renderer_finish_platform_windows(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->finish_platform_windows != nullptr)
        vtable->finish_platform_windows(renderer_state);
}

void
renderer_frame_render(RendererState& renderer_state, ImDrawData* draw_data)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->frame_render != nullptr)
        vtable->frame_render(renderer_state, draw_data);
}

void
renderer_frame_present(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->frame_present != nullptr)
        vtable->frame_present(renderer_state);
}

bool
renderer_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                        unsigned int* pixels, void* user_data)
{
    if (user_data == nullptr)
        return false;
    const RendererState& renderer_state = *static_cast<const RendererState*>(
        user_data);
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->screen_capture == nullptr)
        return false;
    return vtable->screen_capture(viewport_id, x, y, w, h, pixels, user_data);
}

}  // namespace Imiv
