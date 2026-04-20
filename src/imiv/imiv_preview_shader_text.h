// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <string>

namespace Imiv {

extern const char* const k_glsl_preview_fragment_common_functions;
extern const char* const k_metal_basic_preview_uniform_fields;
extern const char* const k_metal_ocio_preview_uniform_fields;
extern const char* const k_metal_preview_common_shader_functions;

std::string
glsl_fullscreen_triangle_vertex_shader();
std::string
glsl_preview_fragment_preamble(bool include_exposure_gamma);
std::string
metal_preview_shader_preamble(const char* preview_uniform_fields);
std::string
metal_fullscreen_triangle_vertex_source(const char* vertex_name);

}  // namespace Imiv
