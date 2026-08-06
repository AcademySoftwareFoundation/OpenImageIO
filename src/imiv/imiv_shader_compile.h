// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Imiv {

enum class RuntimeShaderStage : uint8_t {
    Vertex   = 0,
    Fragment = 1,
    Compute  = 2
};

bool
compile_glsl_to_spirv(RuntimeShaderStage stage, const std::string& source_code,
                      const char* debug_name,
                      std::vector<uint32_t>& spirv_words,
                      std::string& error_message);

}  // namespace Imiv
