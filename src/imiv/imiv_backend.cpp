// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_backend.h"

#include "imiv_build_config.h"
#include "imiv_platform_glfw.h"
#include "imiv_renderer.h"

#include <array>
#include <string>

#include <OpenImageIO/strutil.h>

namespace Imiv {
namespace {

BackendInfo
make_backend_info(BackendKind kind, const char* cli_name,
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

size_t
backend_info_index(BackendKind kind)
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
BackendKind
resolve_backend_with_predicate(BackendKind requested_kind, Predicate predicate)
{
    if (requested_kind != BackendKind::Auto && predicate(requested_kind))
        return requested_kind;

    const BackendKind build_default = active_build_backend_kind();
    if (build_default != BackendKind::Auto && predicate(build_default))
        return build_default;

    const BackendKind platform_default = platform_default_backend_kind();
    if (platform_default != BackendKind::Auto && predicate(platform_default))
        return platform_default;

    if (predicate(BackendKind::Vulkan))
        return BackendKind::Vulkan;
    if (predicate(BackendKind::Metal))
        return BackendKind::Metal;
    if (predicate(BackendKind::OpenGL))
        return BackendKind::OpenGL;

    return BackendKind::Auto;
}

std::array<BackendRuntimeInfo, 3>&
mutable_runtime_backend_info()
{
    static std::array<BackendRuntimeInfo, 3> info;
    return info;
}

bool&
runtime_backend_info_is_valid_flag()
{
    static bool valid = false;
    return valid;
}

void
reset_runtime_backend_info()
{
    std::array<BackendRuntimeInfo, 3>& info = mutable_runtime_backend_info();
    const std::array<BackendInfo, 3>& compiled = compiled_backend_info();
    for (size_t i = 0; i < info.size(); ++i) {
        info[i].build_info          = compiled[i];
        info[i].available           = compiled[i].compiled;
        info[i].probed              = false;
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

}  // namespace Imiv
