// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <array>
#include <string_view>

namespace Imiv {

enum class BackendKind : int { Auto = -1, Vulkan = 0, Metal = 1, OpenGL = 2 };

struct BackendInfo {
    BackendKind kind        = BackendKind::Auto;
    const char* cli_name    = "auto";
    const char* display_name = "Auto";
    bool compiled           = false;
    bool active_build       = false;
    bool platform_default   = false;
};

BackendKind
sanitize_backend_kind(int value);
bool
parse_backend_kind(std::string_view value, BackendKind& out_kind);
const char*
backend_cli_name(BackendKind kind);
const char*
backend_display_name(BackendKind kind);
const char*
backend_runtime_name(BackendKind kind);
BackendKind
active_build_backend_kind();
BackendKind
platform_default_backend_kind();
BackendKind
resolve_backend_request(BackendKind requested_kind);
bool
backend_kind_is_compiled(BackendKind kind);
const std::array<BackendInfo, 3>&
compiled_backend_info();

}  // namespace Imiv
