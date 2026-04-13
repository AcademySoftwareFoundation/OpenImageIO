// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <array>
#include <string>
#include <string_view>

namespace Imiv {

enum class BackendKind : int { Auto = -1, Vulkan = 0, Metal = 1, OpenGL = 2 };

struct BackendInfo {
    BackendKind kind         = BackendKind::Auto;
    const char* cli_name     = "auto";
    const char* display_name = "Auto";
    bool compiled            = false;
    bool active_build        = false;
    bool platform_default    = false;
};

struct BackendRuntimeInfo {
    BackendInfo build_info;
    bool available = false;
    bool probed    = false;
    std::string unavailable_reason;
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
refresh_runtime_backend_info(bool verbose_logging, std::string& error_message);
void
clear_runtime_backend_info();
bool
runtime_backend_info_valid();
bool
backend_kind_is_available(BackendKind kind);
std::string_view
backend_unavailable_reason(BackendKind kind);
bool
backend_kind_is_compiled(BackendKind kind);
const std::array<BackendInfo, 3>&
compiled_backend_info();
const std::array<BackendRuntimeInfo, 3>&
runtime_backend_info();
size_t
compiled_backend_count();

}  // namespace Imiv
