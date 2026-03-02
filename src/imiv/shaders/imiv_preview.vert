#version 450

layout(location = 0) out vec2 uv_out;

void main()
{
    vec2 pos;
    if (gl_VertexIndex == 0)
        pos = vec2(-1.0, -1.0);
    else if (gl_VertexIndex == 1)
        pos = vec2(3.0, -1.0);
    else
        pos = vec2(-1.0, 3.0);

    uv_out = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
