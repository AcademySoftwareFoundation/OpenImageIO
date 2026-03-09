#version 450

layout(location = 0) in vec2 uv_in;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D source_image;

layout(push_constant) uniform PreviewPushConstants {
    float exposure;
    float gamma;
    float offset;
    int color_mode;
    int channel;
    int use_ocio;
    int orientation;
} pc;

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

void main()
{
    vec2 src_uv = display_to_source_uv(uv_in, pc.orientation);
    vec4 c = texture(source_image, src_uv);
    c.rgb += vec3(pc.offset);

    if (pc.color_mode == 1) {
        c.a = 1.0;
    } else if (pc.color_mode == 2) {
        float v = selected_channel(c, pc.channel);
        c = vec4(v, v, v, 1.0);
    } else if (pc.color_mode == 3) {
        float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
        c = vec4(y, y, y, 1.0);
    } else if (pc.color_mode == 4) {
        float v = selected_channel(c, pc.channel);
        c = vec4(heatmap(v), 1.0);
    }

    if (pc.channel > 0 && pc.color_mode != 2 && pc.color_mode != 4) {
        float v = selected_channel(c, pc.channel);
        c = vec4(v, v, v, 1.0);
    }

    float exposure_scale = exp2(pc.exposure);
    c.rgb *= exposure_scale;
    float g = max(pc.gamma, 0.01);
    c.rgb = pow(max(c.rgb, vec3(0.0)), vec3(1.0 / g));

    // OCIO pipeline is not connected yet; keep this branch explicit.
    if (pc.use_ocio != 0) {
        c.rgb = c.rgb;
    }

    out_color = c;
}
