// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_platform_glfw.h"
#include "imiv_viewer.h"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_opengl3_loader.h>

#include <OpenImageIO/imagebuf.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#ifndef GL_TEXTURE_BASE_LEVEL
#    define GL_TEXTURE_BASE_LEVEL 0x813C
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#    define GL_TEXTURE_MAX_LEVEL 0x813D
#endif
#ifndef GL_RGBA32F
#    define GL_RGBA32F 0x8814
#endif
#ifndef GL_NO_ERROR
#    define GL_NO_ERROR 0
#endif
#ifndef GL_FRAMEBUFFER
#    define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#    define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#    define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_VERTEX_SHADER
#    define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#    define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#    define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#    define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#    define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#    define GL_TEXTURE0 0x84C0
#endif

namespace Imiv {

struct RendererTextureBackendState {
    GLuint source_texture                         = 0;
    GLuint preview_linear_texture                 = 0;
    GLuint preview_nearest_texture                = 0;
    int width                                     = 0;
    int height                                    = 0;
    int input_channels                            = 0;
    bool preview_dirty                            = true;
    bool preview_params_valid                     = false;
    RendererPreviewControls last_preview_controls = {};
};

struct BasicPreviewProgram {
    GLuint program                 = 0;
    GLuint fullscreen_triangle_vao = 0;
    GLint source_sampler_location  = -1;
    GLint input_channels_location  = -1;
    GLint exposure_location        = -1;
    GLint gamma_location           = -1;
    GLint offset_location          = -1;
    GLint color_mode_location      = -1;
    GLint channel_location         = -1;
    GLint orientation_location     = -1;
    bool ready                     = false;
};

struct OcioPreviewProgram {
    bool ready = false;
};

using GlUniform1fProc       = void(APIENTRY*)(GLint location, GLfloat v0);
using GlDrawArraysProc      = void(APIENTRY*)(GLenum mode, GLint first,
                                         GLsizei count);
using GlGenFramebuffersProc = void(APIENTRY*)(GLsizei n, GLuint* framebuffers);
using GlBindFramebufferProc = void(APIENTRY*)(GLenum target,
                                              GLuint framebuffer);
using GlDeleteFramebuffersProc = void(APIENTRY*)(GLsizei n,
                                                 const GLuint* framebuffers);
using GlFramebufferTexture2DProc
    = void(APIENTRY*)(GLenum target, GLenum attachment, GLenum textarget,
                      GLuint texture, GLint level);
using GlCheckFramebufferStatusProc = GLenum(APIENTRY*)(GLenum target);

struct OpenGlExtraProcs {
    GlUniform1fProc Uniform1f                           = nullptr;
    GlDrawArraysProc DrawArrays                         = nullptr;
    GlGenFramebuffersProc GenFramebuffers               = nullptr;
    GlBindFramebufferProc BindFramebuffer               = nullptr;
    GlDeleteFramebuffersProc DeleteFramebuffers         = nullptr;
    GlFramebufferTexture2DProc FramebufferTexture2D     = nullptr;
    GlCheckFramebufferStatusProc CheckFramebufferStatus = nullptr;
    bool ready                                          = false;
};

struct RendererBackendState {
    GLFWwindow* window         = nullptr;
    GLFWwindow* backup_context = nullptr;
    const char* glsl_version   = nullptr;
    bool imgui_initialized     = false;
    OpenGlExtraProcs extra_procs;
    GLuint preview_framebuffer = 0;
    BasicPreviewProgram basic_preview;
    OcioPreviewProgram ocio_preview;
};

namespace {

    RendererBackendState* backend_state(RendererState& renderer_state)
    {
        return static_cast<RendererBackendState*>(renderer_state.backend);
    }

    const RendererTextureBackendState*
    texture_backend_state(const RendererTexture& texture)
    {
        return static_cast<const RendererTextureBackendState*>(texture.backend);
    }

    RendererTextureBackendState* texture_backend_state(RendererTexture& texture)
    {
        return static_cast<RendererTextureBackendState*>(texture.backend);
    }

    bool ensure_backend_state(RendererState& renderer_state)
    {
        if (renderer_state.backend != nullptr)
            return true;
        renderer_state.backend = new RendererBackendState();
        return renderer_state.backend != nullptr;
    }

    const char* open_gl_glsl_version()
    {
#if defined(__APPLE__)
        return "#version 150";
#else
        return "#version 130";
#endif
    }

    bool preview_controls_equal(const RendererPreviewControls& a,
                                const RendererPreviewControls& b)
    {
        return std::abs(a.exposure - b.exposure) < 1.0e-6f
               && std::abs(a.gamma - b.gamma) < 1.0e-6f
               && std::abs(a.offset - b.offset) < 1.0e-6f
               && a.color_mode == b.color_mode && a.channel == b.channel
               && a.orientation == b.orientation;
    }

    template<class ProcT>
    bool load_gl_proc(const char* name, ProcT& proc, std::string& error_message)
    {
        proc = reinterpret_cast<ProcT>(platform_glfw_get_proc_address(name));
        if (proc != nullptr)
            return true;
        error_message = std::string("missing OpenGL function: ") + name;
        return false;
    }

    bool ensure_extra_procs(RendererBackendState& state,
                            std::string& error_message)
    {
        if (state.extra_procs.ready)
            return true;
        return load_gl_proc("glUniform1f", state.extra_procs.Uniform1f,
                            error_message)
               && load_gl_proc("glDrawArrays", state.extra_procs.DrawArrays,
                               error_message)
               && load_gl_proc("glGenFramebuffers",
                               state.extra_procs.GenFramebuffers, error_message)
               && load_gl_proc("glBindFramebuffer",
                               state.extra_procs.BindFramebuffer, error_message)
               && load_gl_proc("glDeleteFramebuffers",
                               state.extra_procs.DeleteFramebuffers,
                               error_message)
               && load_gl_proc("glFramebufferTexture2D",
                               state.extra_procs.FramebufferTexture2D,
                               error_message)
               && load_gl_proc("glCheckFramebufferStatus",
                               state.extra_procs.CheckFramebufferStatus,
                               error_message)
               && ((state.extra_procs.ready = true), true);
    }

    bool compile_shader(GLuint shader, const char* const* sources,
                        GLsizei source_count, std::string& error_message)
    {
        glShaderSource(shader, source_count, sources, nullptr);
        glCompileShader(shader);
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_TRUE) {
            error_message.clear();
            return true;
        }

        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log(std::max(0, log_length), '\0');
        if (log_length > 0)
            glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        error_message = log.empty() ? "OpenGL shader compilation failed" : log;
        return false;
    }

    bool link_program(GLuint program, std::string& error_message)
    {
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_TRUE) {
            error_message.clear();
            return true;
        }

        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log(std::max(0, log_length), '\0');
        if (log_length > 0)
            glGetProgramInfoLog(program, log_length, nullptr, log.data());
        error_message = log.empty() ? "OpenGL program link failed" : log;
        return false;
    }

    void destroy_basic_preview_program(BasicPreviewProgram& program)
    {
        if (program.fullscreen_triangle_vao != 0) {
            glDeleteVertexArrays(1, &program.fullscreen_triangle_vao);
            program.fullscreen_triangle_vao = 0;
        }
        if (program.program != 0) {
            glDeleteProgram(program.program);
            program.program = 0;
        }
        program.source_sampler_location = -1;
        program.input_channels_location = -1;
        program.exposure_location       = -1;
        program.gamma_location          = -1;
        program.offset_location         = -1;
        program.color_mode_location     = -1;
        program.channel_location        = -1;
        program.orientation_location    = -1;
        program.ready                   = false;
    }

    bool ensure_basic_preview_program(RendererBackendState& state,
                                      std::string& error_message)
    {
        if (state.basic_preview.ready)
            return true;
        if (!ensure_extra_procs(state, error_message))
            return false;

        static const char* vertex_source = R"glsl(
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

        static const char* fragment_source = R"glsl(
in vec2 uv_in;
out vec4 out_color;

uniform sampler2D u_source_image;
uniform int u_input_channels;
uniform float u_exposure;
uniform float u_gamma;
uniform float u_offset;
uniform int u_color_mode;
uniform int u_channel;
uniform int u_orientation;

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
    vec2 src_uv = display_to_source_uv(uv_in, u_orientation);
    vec4 c = texture(u_source_image, src_uv);
    c.rgb += vec3(u_offset);

    if (u_color_mode == 1) {
        c.a = 1.0;
    } else if (u_color_mode == 2) {
        float v = selected_channel(c, u_channel);
        c = vec4(v, v, v, 1.0);
    } else if (u_color_mode == 3) {
        float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
        c = vec4(y, y, y, 1.0);
    } else if (u_color_mode == 4) {
        float v = selected_channel(c, u_channel);
        c = vec4(heatmap(v), 1.0);
    }

    if (u_channel > 0 && u_color_mode != 2 && u_color_mode != 4) {
        float v = selected_channel(c, u_channel);
        c = vec4(v, v, v, 1.0);
    }

    if (u_input_channels == 1 && u_color_mode <= 1)
        c = vec4(c.rrr, 1.0);
    else if (u_input_channels == 2 && u_color_mode == 0)
        c = vec4(c.rrr, c.a);
    else if (u_input_channels == 2 && u_color_mode == 1)
        c = vec4(c.rrr, 1.0);

    float exposure_scale = exp2(u_exposure);
    c.rgb *= exposure_scale;
    float g = max(u_gamma, 0.01);
    c.rgb = pow(max(c.rgb, vec3(0.0)), vec3(1.0 / g));

    out_color = c;
}
)glsl";

        GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        if (vertex_shader == 0 || fragment_shader == 0) {
            if (vertex_shader != 0)
                glDeleteShader(vertex_shader);
            if (fragment_shader != 0)
                glDeleteShader(fragment_shader);
            error_message = "failed to create OpenGL shader objects";
            return false;
        }

        const char* vertex_sources[]   = { state.glsl_version, "\n",
                                           vertex_source };
        const char* fragment_sources[] = { state.glsl_version, "\n",
                                           fragment_source };
        bool ok = compile_shader(vertex_shader, vertex_sources, 3,
                                 error_message)
                  && compile_shader(fragment_shader, fragment_sources, 3,
                                    error_message);
        if (!ok) {
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return false;
        }

        GLuint program = glCreateProgram();
        if (program == 0) {
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            error_message = "failed to create OpenGL preview program";
            return false;
        }
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        ok = link_program(program, error_message);
        glDetachShader(program, vertex_shader);
        glDetachShader(program, fragment_shader);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        if (!ok) {
            glDeleteProgram(program);
            return false;
        }

        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        if (vao == 0) {
            glDeleteProgram(program);
            error_message = "failed to create OpenGL preview VAO";
            return false;
        }

        state.basic_preview.program                 = program;
        state.basic_preview.fullscreen_triangle_vao = vao;
        state.basic_preview.source_sampler_location
            = glGetUniformLocation(program, "u_source_image");
        state.basic_preview.input_channels_location
            = glGetUniformLocation(program, "u_input_channels");
        state.basic_preview.exposure_location
            = glGetUniformLocation(program, "u_exposure");
        state.basic_preview.gamma_location  = glGetUniformLocation(program,
                                                                   "u_gamma");
        state.basic_preview.offset_location = glGetUniformLocation(program,
                                                                   "u_offset");
        state.basic_preview.color_mode_location
            = glGetUniformLocation(program, "u_color_mode");
        state.basic_preview.channel_location
            = glGetUniformLocation(program, "u_channel");
        state.basic_preview.orientation_location
            = glGetUniformLocation(program, "u_orientation");
        state.basic_preview.ready = true;
        error_message.clear();
        return true;
    }

    bool ensure_preview_framebuffer(RendererBackendState& state,
                                    std::string& error_message)
    {
        if (state.preview_framebuffer != 0)
            return true;
        if (!ensure_extra_procs(state, error_message))
            return false;
        state.extra_procs.GenFramebuffers(1, &state.preview_framebuffer);
        if (state.preview_framebuffer != 0) {
            error_message.clear();
            return true;
        }
        error_message = "failed to create OpenGL preview framebuffer";
        return false;
    }

    bool allocate_texture_storage(GLuint texture_id, GLint filter, int width,
                                  int height, const float* rgba_pixels,
                                  std::string& error_message)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                     GL_FLOAT, rgba_pixels);
        const GLenum err = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        if (err == GL_NO_ERROR) {
            error_message.clear();
            return true;
        }
        error_message = "OpenGL texture upload failed";
        return false;
    }

    bool render_preview_texture(RendererBackendState& state,
                                RendererTextureBackendState& texture_state,
                                GLuint target_texture,
                                const RendererPreviewControls& controls,
                                std::string& error_message)
    {
        if (!ensure_basic_preview_program(state, error_message)
            || !ensure_preview_framebuffer(state, error_message)) {
            return false;
        }

        state.extra_procs.BindFramebuffer(GL_FRAMEBUFFER,
                                          state.preview_framebuffer);
        state.extra_procs.FramebufferTexture2D(GL_FRAMEBUFFER,
                                               GL_COLOR_ATTACHMENT0,
                                               GL_TEXTURE_2D, target_texture,
                                               0);
        if (state.extra_procs.CheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
            state.extra_procs.BindFramebuffer(GL_FRAMEBUFFER, 0);
            error_message = "OpenGL preview framebuffer is incomplete";
            return false;
        }

        glViewport(0, 0, texture_state.width, texture_state.height);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(state.basic_preview.program);
        glBindVertexArray(state.basic_preview.fullscreen_triangle_vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_state.source_texture);
        glUniform1i(state.basic_preview.source_sampler_location, 0);
        glUniform1i(state.basic_preview.input_channels_location,
                    texture_state.input_channels);
        state.extra_procs.Uniform1f(state.basic_preview.exposure_location,
                                    controls.exposure);
        state.extra_procs.Uniform1f(state.basic_preview.gamma_location,
                                    std::max(0.01f, controls.gamma));
        state.extra_procs.Uniform1f(state.basic_preview.offset_location,
                                    controls.offset);
        glUniform1i(state.basic_preview.color_mode_location,
                    controls.color_mode);
        glUniform1i(state.basic_preview.channel_location, controls.channel);
        glUniform1i(state.basic_preview.orientation_location,
                    controls.orientation);
        state.extra_procs.DrawArrays(GL_TRIANGLES, 0, 3);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        state.extra_procs.BindFramebuffer(GL_FRAMEBUFFER, 0);
        const GLenum err = glGetError();
        if (err == GL_NO_ERROR) {
            error_message.clear();
            return true;
        }
        error_message = "OpenGL preview draw failed";
        return false;
    }

    bool build_rgba_float_pixels(const LoadedImage& image,
                                 std::vector<float>& rgba_pixels,
                                 std::string& error_message)
    {
        using namespace OIIO;

        error_message.clear();
        rgba_pixels.clear();
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
            error_message = "invalid source image dimensions";
            return false;
        }

        const TypeDesc format = upload_data_type_to_typedesc(image.type);
        if (format == TypeUnknown) {
            error_message = "unsupported source pixel type";
            return false;
        }

        const size_t width         = static_cast<size_t>(image.width);
        const size_t height        = static_cast<size_t>(image.height);
        const size_t channels      = static_cast<size_t>(image.nchannels);
        const size_t min_row_pitch = width * channels * image.channel_bytes;
        if (image.row_pitch_bytes < min_row_pitch) {
            error_message = "invalid source row pitch";
            return false;
        }

        ImageSpec spec(image.width, image.height, image.nchannels, format);
        ImageBuf source(spec);
        const auto* begin = reinterpret_cast<const std::byte*>(
            image.pixels.data());
        const cspan<std::byte> byte_span(begin, image.pixels.size());
        const stride_t xstride = static_cast<stride_t>(image.nchannels
                                                       * image.channel_bytes);
        const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
        if (!source.set_pixels(ROI::All(), format, byte_span, begin, xstride,
                               ystride, AutoStride)) {
            error_message = source.geterror().empty()
                                ? "failed to stage image data"
                                : source.geterror();
            return false;
        }

        std::vector<float> source_pixels(width * height * channels, 0.0f);
        if (!source.get_pixels(ROI::All(), TypeFloat, source_pixels.data())) {
            error_message = source.geterror().empty()
                                ? "failed to convert image pixels to float"
                                : source.geterror();
            return false;
        }

        rgba_pixels.assign(width * height * 4, 1.0f);
        for (size_t pixel = 0, src = 0; pixel < width * height; ++pixel) {
            float* dst = &rgba_pixels[pixel * 4];
            if (channels == 1) {
                dst[0] = source_pixels[src + 0];
                dst[1] = source_pixels[src + 0];
                dst[2] = source_pixels[src + 0];
                dst[3] = 1.0f;
            } else if (channels == 2) {
                dst[0] = source_pixels[src + 0];
                dst[1] = source_pixels[src + 0];
                dst[2] = source_pixels[src + 0];
                dst[3] = source_pixels[src + 1];
            } else {
                dst[0] = source_pixels[src + 0];
                dst[1] = source_pixels[src + 1];
                dst[2] = source_pixels[src + 2];
                dst[3] = (channels >= 4) ? source_pixels[src + 3] : 1.0f;
            }
            src += channels;
        }
        return true;
    }

}  // namespace

bool
renderer_backend_get_viewer_texture_refs(const ViewerState& viewer,
                                         const PlaceholderUiState& ui_state,
                                         ImTextureRef& main_texture_ref,
                                         bool& has_main_texture,
                                         ImTextureRef& closeup_texture_ref,
                                         bool& has_closeup_texture)
{
    const RendererTextureBackendState* state = texture_backend_state(
        viewer.texture);
    if (state == nullptr || !viewer.texture.preview_initialized)
        return false;

    const GLuint main_texture = ui_state.linear_interpolation
                                    ? state->preview_linear_texture
                                    : state->preview_nearest_texture;
    if (main_texture != 0) {
        main_texture_ref = ImTextureRef(
            static_cast<ImTextureID>(static_cast<intptr_t>(main_texture)));
        has_main_texture = true;
    }

    const GLuint closeup_texture = state->preview_nearest_texture != 0
                                       ? state->preview_nearest_texture
                                       : state->preview_linear_texture;
    if (closeup_texture != 0) {
        closeup_texture_ref = ImTextureRef(
            static_cast<ImTextureID>(static_cast<intptr_t>(closeup_texture)));
        has_closeup_texture = true;
    }
    return has_main_texture || has_closeup_texture;
}

bool
renderer_backend_create_texture(RendererState& renderer_state,
                                const LoadedImage& image,
                                RendererTexture& texture,
                                std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr) {
        error_message = "OpenGL window is not initialized";
        return false;
    }

    std::vector<float> rgba_pixels;
    if (!build_rgba_float_pixels(image, rgba_pixels, error_message))
        return false;

    auto* texture_state = new RendererTextureBackendState();
    if (texture_state == nullptr) {
        error_message = "failed to allocate OpenGL texture state";
        return false;
    }

    platform_glfw_make_context_current(state->window);
    glGenTextures(1, &texture_state->source_texture);
    glGenTextures(1, &texture_state->preview_linear_texture);
    glGenTextures(1, &texture_state->preview_nearest_texture);
    texture_state->width          = image.width;
    texture_state->height         = image.height;
    texture_state->input_channels = image.nchannels;
    texture_state->preview_dirty  = true;
    if (texture_state->source_texture == 0
        || texture_state->preview_linear_texture == 0
        || texture_state->preview_nearest_texture == 0
        || !allocate_texture_storage(texture_state->source_texture, GL_NEAREST,
                                     image.width, image.height,
                                     rgba_pixels.data(), error_message)
        || !allocate_texture_storage(texture_state->preview_linear_texture,
                                     GL_LINEAR, image.width, image.height,
                                     nullptr, error_message)
        || !allocate_texture_storage(texture_state->preview_nearest_texture,
                                     GL_NEAREST, image.width, image.height,
                                     nullptr, error_message)) {
        if (texture_state->source_texture != 0)
            glDeleteTextures(1, &texture_state->source_texture);
        if (texture_state->preview_linear_texture != 0)
            glDeleteTextures(1, &texture_state->preview_linear_texture);
        if (texture_state->preview_nearest_texture != 0)
            glDeleteTextures(1, &texture_state->preview_nearest_texture);
        delete texture_state;
        return false;
    }

    texture.backend             = texture_state;
    texture.preview_initialized = false;
    error_message.clear();
    return true;
}

void
renderer_backend_destroy_texture(RendererState& renderer_state,
                                 RendererTexture& texture)
{
    RendererTextureBackendState* state = texture_backend_state(texture);
    RendererBackendState* renderer     = backend_state(renderer_state);
    if (state == nullptr) {
        texture.preview_initialized = false;
        return;
    }
    if (renderer != nullptr && renderer->window != nullptr)
        platform_glfw_make_context_current(renderer->window);
    if (state->source_texture != 0)
        glDeleteTextures(1, &state->source_texture);
    if (state->preview_linear_texture != 0)
        glDeleteTextures(1, &state->preview_linear_texture);
    if (state->preview_nearest_texture != 0)
        glDeleteTextures(1, &state->preview_nearest_texture);
    delete state;
    texture.backend             = nullptr;
    texture.preview_initialized = false;
}

bool
renderer_backend_update_preview_texture(RendererState& renderer_state,
                                        RendererTexture& texture,
                                        const LoadedImage* image,
                                        const PlaceholderUiState& ui_state,
                                        const RendererPreviewControls& controls,
                                        std::string& error_message)
{
    (void)image;
    (void)ui_state;

    RendererBackendState* state                = backend_state(renderer_state);
    RendererTextureBackendState* texture_state = texture_backend_state(texture);
    if (state == nullptr || state->window == nullptr
        || texture_state == nullptr) {
        error_message = "OpenGL preview state is not initialized";
        return false;
    }

    if (!texture_state->preview_dirty && texture_state->preview_params_valid
        && preview_controls_equal(texture_state->last_preview_controls,
                                  controls)) {
        texture.preview_initialized = true;
        error_message.clear();
        return true;
    }

    platform_glfw_make_context_current(state->window);
    if (!render_preview_texture(*state, *texture_state,
                                texture_state->preview_linear_texture, controls,
                                error_message)
        || !render_preview_texture(*state, *texture_state,
                                   texture_state->preview_nearest_texture,
                                   controls, error_message)) {
        texture.preview_initialized = false;
        return false;
    }

    texture_state->preview_dirty         = false;
    texture_state->preview_params_valid  = true;
    texture_state->last_preview_controls = controls;
    texture.preview_initialized          = true;
    error_message.clear();
    return true;
}

bool
renderer_backend_quiesce_texture_preview_submission(
    RendererState& renderer_state, RendererTexture& texture,
    std::string& error_message)
{
    (void)renderer_state;
    (void)texture;
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_instance(RendererState& renderer_state,
                                ImVector<const char*>& instance_extensions,
                                std::string& error_message)
{
    (void)instance_extensions;
    if (!ensure_backend_state(renderer_state)) {
        error_message = "failed to allocate OpenGL renderer state";
        return false;
    }
    backend_state(renderer_state)->glsl_version = open_gl_glsl_version();
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_device(RendererState& renderer_state,
                              std::string& error_message)
{
    if (backend_state(renderer_state) == nullptr) {
        error_message = "OpenGL renderer state is not initialized";
        return false;
    }
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_window(RendererState& renderer_state, int width,
                              int height, std::string& error_message)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    error_message.clear();
    return true;
}

bool
renderer_backend_create_surface(RendererState& renderer_state,
                                GLFWwindow* window, std::string& error_message)
{
    if (!ensure_backend_state(renderer_state)) {
        error_message = "failed to allocate OpenGL renderer state";
        return false;
    }
    backend_state(renderer_state)->window = window;
    error_message.clear();
    return true;
}

void
renderer_backend_destroy_surface(RendererState& renderer_state)
{
    if (RendererBackendState* state = backend_state(renderer_state))
        state->window = nullptr;
}

void
renderer_backend_cleanup_window(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_cleanup(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state != nullptr) {
        if (state->window != nullptr)
            platform_glfw_make_context_current(state->window);
        destroy_basic_preview_program(state->basic_preview);
        if (state->preview_framebuffer != 0 && state->extra_procs.ready) {
            state->extra_procs.DeleteFramebuffers(1,
                                                  &state->preview_framebuffer);
            state->preview_framebuffer = 0;
        }
    }
    delete state;
    renderer_state.backend = nullptr;
}

bool
renderer_backend_wait_idle(RendererState& renderer_state,
                           std::string& error_message)
{
    (void)renderer_state;
    error_message.clear();
    return true;
}

bool
renderer_backend_imgui_init(RendererState& renderer_state,
                            std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr) {
        error_message = "OpenGL window is not initialized";
        return false;
    }
    platform_glfw_make_context_current(state->window);
    if (ImGui_ImplOpenGL3_Init(state->glsl_version)) {
        state->imgui_initialized = true;
        error_message.clear();
        return true;
    }
    error_message = "ImGui_ImplOpenGL3_Init failed";
    return false;
}

void
renderer_backend_imgui_shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
}

void
renderer_backend_imgui_new_frame(RendererState& renderer_state)
{
    (void)renderer_state;
    ImGui_ImplOpenGL3_NewFrame();
}

bool
renderer_backend_needs_main_window_resize(RendererState& renderer_state,
                                          int width, int height)
{
    return renderer_state.framebuffer_width != width
           || renderer_state.framebuffer_height != height;
}

void
renderer_backend_resize_main_window(RendererState& renderer_state, int width,
                                    int height)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
}

void
renderer_backend_set_main_clear_color(RendererState& renderer_state, float r,
                                      float g, float b, float a)
{
    renderer_state.clear_color[0] = r;
    renderer_state.clear_color[1] = g;
    renderer_state.clear_color[2] = b;
    renderer_state.clear_color[3] = a;
}

void
renderer_backend_prepare_platform_windows(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr)
        return;
    state->backup_context = platform_glfw_get_current_context();
}

void
renderer_backend_finish_platform_windows(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->backup_context == nullptr)
        return;
    platform_glfw_make_context_current(state->backup_context);
    state->backup_context = nullptr;
}

void
renderer_backend_frame_render(RendererState& renderer_state,
                              ImDrawData* draw_data)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr)
        return;

    int display_w = 0;
    int display_h = 0;
    platform_glfw_get_framebuffer_size(state->window, display_w, display_h);
    renderer_state.framebuffer_width  = display_w;
    renderer_state.framebuffer_height = display_h;
    glViewport(0, 0, display_w, display_h);
    glClearColor(renderer_state.clear_color[0] * renderer_state.clear_color[3],
                 renderer_state.clear_color[1] * renderer_state.clear_color[3],
                 renderer_state.clear_color[2] * renderer_state.clear_color[3],
                 renderer_state.clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

void
renderer_backend_frame_present(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr)
        return;
    platform_glfw_swap_buffers(state->window);
}

bool
renderer_backend_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                                unsigned int* pixels, void* user_data)
{
    (void)viewport_id;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)pixels;
    (void)user_data;
    return false;
}

}  // namespace Imiv
