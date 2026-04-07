// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_preview_shader_text.h"

namespace Imiv {

std::string
glsl_fullscreen_triangle_vertex_shader()
{
    return R"glsl(
out vec2 uv_in;

void main()
{
    vec2 pos = vec2(-1.0, -1.0);
    if (gl_VertexID == 1)
        pos = vec2(3.0, -1.0);
    else if (gl_VertexID == 2)
        pos = vec2(-1.0, 3.0);

    uv_in = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";
}

std::string
glsl_preview_fragment_preamble(bool include_exposure_gamma)
{
    std::string source = R"glsl(
in vec2 uv_in;
out vec4 out_color;

uniform sampler2D u_source_image;
uniform int u_input_channels;
)glsl";
    if (include_exposure_gamma) {
        source += R"glsl(
uniform float u_exposure;
uniform float u_gamma;
)glsl";
    }
    source += R"glsl(
uniform float u_offset;
uniform int u_color_mode;
uniform int u_channel;
uniform int u_orientation;

)glsl";
    source += glsl_preview_fragment_common_functions();
    return source;
}

const char*
glsl_preview_fragment_common_functions()
{
    return R"glsl(
vec2 display_to_source_uv(vec2 uv, int orientation)
{
    if (orientation == 2)
        return vec2(1.0 - uv.x, uv.y);
    if (orientation == 3)
        return vec2(1.0 - uv.x, 1.0 - uv.y);
    if (orientation == 4)
        return vec2(uv.x, 1.0 - uv.y);
    if (orientation == 5)
        return vec2(uv.y, uv.x);
    if (orientation == 6)
        return vec2(uv.y, 1.0 - uv.x);
    if (orientation == 7)
        return vec2(1.0 - uv.y, 1.0 - uv.x);
    if (orientation == 8)
        return vec2(1.0 - uv.y, uv.x);
    return uv;
}

float selected_channel(vec4 c, int channel)
{
    if (channel == 1)
        return c.r;
    if (channel == 2)
        return c.g;
    if (channel == 3)
        return c.b;
    if (channel == 4)
        return c.a;
    return c.r;
}

vec3 heatmap(float x)
{
    float t = clamp(x, 0.0, 1.0);
    vec3 a = vec3(0.0, 0.0, 0.5);
    vec3 b = vec3(0.0, 0.9, 1.0);
    vec3 c = vec3(1.0, 1.0, 0.0);
    vec3 d = vec3(1.0, 0.0, 0.0);
    if (t < 0.33)
        return mix(a, b, t / 0.33);
    if (t < 0.66)
        return mix(b, c, (t - 0.33) / 0.33);
    return mix(c, d, (t - 0.66) / 0.34);
}
)glsl";
}

const char*
metal_basic_preview_uniform_fields()
{
    return R"metal(
    float exposure;
    float gamma;
    float offset;
    int color_mode;
    int channel;
    int input_channels;
    int orientation;
    int _padding;
)metal";
}

const char*
metal_ocio_preview_uniform_fields()
{
    return R"metal(
    float offset;
    int color_mode;
    int channel;
    int input_channels;
    int orientation;
)metal";
}

std::string
metal_preview_shader_preamble(const char* preview_uniform_fields)
{
    std::string source = R"metal(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

struct PreviewUniforms {
)metal";
    if (preview_uniform_fields != nullptr)
        source += preview_uniform_fields;
    source += R"metal(
};
)metal";
    return source;
}

std::string
metal_fullscreen_triangle_vertex_source(const char* vertex_name)
{
    const char* name   = (vertex_name != nullptr && vertex_name[0] != '\0')
                             ? vertex_name
                             : "imivPreviewVertex";
    std::string source = "\nvertex VertexOut ";
    source += name;
    source += R"metal((uint vertex_id [[vertex_id]])
{
    const float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    const float2 uvs[3] = { float2(0.0, 0.0), float2(2.0, 0.0), float2(0.0, 2.0) };
    VertexOut out;
    out.position = float4(positions[vertex_id], 0.0, 1.0);
    out.uv = uvs[vertex_id];
    return out;
}
)metal";
    return source;
}

const char*
metal_preview_common_shader_functions()
{
    return R"metal(
inline float selected_channel(float4 rgba, int channel)
{
    if (channel == 1) return rgba.r;
    if (channel == 2) return rgba.g;
    if (channel == 3) return rgba.b;
    if (channel == 4) return rgba.a;
    return rgba.r;
}

inline float3 heatmap(float x)
{
    float t = clamp(x, 0.0, 1.0);
    float3 a = float3(0.0, 0.0, 0.5);
    float3 b = float3(0.0, 0.9, 1.0);
    float3 c = float3(1.0, 1.0, 0.0);
    float3 d = float3(1.0, 0.0, 0.0);
    if (t < 0.33)
        return mix(a, b, t / 0.33);
    if (t < 0.66)
        return mix(b, c, (t - 0.33) / 0.33);
    return mix(c, d, (t - 0.66) / 0.34);
}

inline float2 display_to_source_uv(float2 uv, int orientation)
{
    switch (orientation) {
    case 2: return float2(1.0 - uv.x, uv.y);
    case 3: return float2(1.0 - uv.x, 1.0 - uv.y);
    case 4: return float2(uv.x, 1.0 - uv.y);
    case 5: return float2(uv.y, uv.x);
    case 6: return float2(uv.y, 1.0 - uv.x);
    case 7: return float2(1.0 - uv.y, 1.0 - uv.x);
    case 8: return float2(1.0 - uv.y, uv.x);
    default: return uv;
    }
}
)metal";
}

}  // namespace Imiv
