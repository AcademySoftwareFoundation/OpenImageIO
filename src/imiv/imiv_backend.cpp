// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_backend.h"

#include "imiv_build_config.h"

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
    if (requested_kind != BackendKind::Auto
        && backend_kind_is_compiled(requested_kind)) {
        return requested_kind;
    }

    const BackendKind build_default = active_build_backend_kind();
    if (build_default != BackendKind::Auto
        && backend_kind_is_compiled(build_default)) {
        return build_default;
    }

    const BackendKind platform_default = platform_default_backend_kind();
    if (platform_default != BackendKind::Auto
        && backend_kind_is_compiled(platform_default)) {
        return platform_default;
    }

    if (backend_kind_is_compiled(BackendKind::Vulkan))
        return BackendKind::Vulkan;
    if (backend_kind_is_compiled(BackendKind::Metal))
        return BackendKind::Metal;
    if (backend_kind_is_compiled(BackendKind::OpenGL))
        return BackendKind::OpenGL;

    return BackendKind::Auto;
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
