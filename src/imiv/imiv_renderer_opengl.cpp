// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_ocio.h"
#include "imiv_platform_glfw.h"
#include "imiv_viewer.h"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_opengl3_loader.h>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/strutil.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
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
#ifndef GL_UNPACK_ROW_LENGTH
#    define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_TEXTURE_1D
#    define GL_TEXTURE_1D 0x0DE0
#endif
#ifndef GL_TEXTURE_3D
#    define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_R32F
#    define GL_R32F 0x822E
#endif
#ifndef GL_R8
#    define GL_R8 0x8229
#endif
#ifndef GL_R16
#    define GL_R16 0x822A
#endif
#ifndef GL_R16F
#    define GL_R16F 0x822D
#endif
#ifndef GL_RG
#    define GL_RG 0x8227
#endif
#ifndef GL_RG8
#    define GL_RG8 0x822B
#endif
#ifndef GL_RG16
#    define GL_RG16 0x822C
#endif
#ifndef GL_RG16F
#    define GL_RG16F 0x822F
#endif
#ifndef GL_RG32F
#    define GL_RG32F 0x8230
#endif
#ifndef GL_RGB32F
#    define GL_RGB32F 0x8815
#endif
#ifndef GL_RGB8
#    define GL_RGB8 0x8051
#endif
#ifndef GL_RGB16
#    define GL_RGB16 0x8054
#endif
#ifndef GL_RGB16F
#    define GL_RGB16F 0x881B
#endif
#ifndef GL_RGBA8
#    define GL_RGBA8 0x8058
#endif
#ifndef GL_RGBA16
#    define GL_RGBA16 0x805B
#endif
#ifndef GL_RGBA16F
#    define GL_RGBA16F 0x881A
#endif
#ifndef GL_HALF_FLOAT
#    define GL_HALF_FLOAT 0x140B
#endif
#ifndef GL_TEXTURE_WRAP_R
#    define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_RED
#    define GL_RED 0x1903
#endif
#ifndef GL_RGB
#    define GL_RGB 0x1907
#endif

namespace Imiv {

namespace {

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
    std::string last_ocio_shader_cache_id;
};

struct SourceTextureUploadDesc {
    GLint internal_format                 = GL_RGBA32F;
    GLenum format                         = GL_RGBA;
    GLenum type                           = GL_FLOAT;
    const void* pixels                    = nullptr;
    GLint unpack_row_length               = 0;
    std::vector<float> fallback_rgba_data = {};
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
    struct TextureDesc {
        GLuint texture_id      = 0;
        GLenum target          = 0;
        GLint sampler_location = -1;
        GLint texture_unit     = 0;
    };

    struct UniformDesc {
        std::string name;
        OCIO::GpuShaderDesc::UniformData data;
        GLint location = -1;
    };

    GLuint program                = 0;
    GLint source_sampler_location = -1;
    GLint input_channels_location = -1;
    GLint offset_location         = -1;
    GLint color_mode_location     = -1;
    GLint channel_location        = -1;
    GLint orientation_location    = -1;
    OcioShaderRuntime* runtime    = nullptr;
    std::string shader_cache_id;
    std::vector<TextureDesc> textures;
    std::vector<UniformDesc> uniforms;
    bool ready = false;
};

using GlUniform1fProc  = void(APIENTRY*)(GLint location, GLfloat v0);
using GlUniform3fProc  = void(APIENTRY*)(GLint location, GLfloat v0, GLfloat v1,
                                        GLfloat v2);
using GlUniform1fvProc = void(APIENTRY*)(GLint location, GLsizei count,
                                         const GLfloat* value);
using GlUniform1ivProc = void(APIENTRY*)(GLint location, GLsizei count,
                                         const GLint* value);
using GlDrawArraysProc = void(APIENTRY*)(GLenum mode, GLint first,
                                         GLsizei count);
using GlTexImage1DProc = void(APIENTRY*)(GLenum target, GLint level,
                                         GLint internalformat, GLsizei width,
                                         GLint border, GLenum format,
                                         GLenum type, const void* pixels);
using GlGenFramebuffersProc = void(APIENTRY*)(GLsizei n, GLuint* framebuffers);
using GlBindFramebufferProc = void(APIENTRY*)(GLenum target,
                                              GLuint framebuffer);
using GlDeleteFramebuffersProc = void(APIENTRY*)(GLsizei n,
                                                 const GLuint* framebuffers);
using GlFramebufferTexture2DProc
    = void(APIENTRY*)(GLenum target, GLenum attachment, GLenum textarget,
                      GLuint texture, GLint level);
using GlCheckFramebufferStatusProc = GLenum(APIENTRY*)(GLenum target);
using GlTexImage3DProc             = void(APIENTRY*)(GLenum target, GLint level,
                                         GLint internalformat, GLsizei width,
                                         GLsizei height, GLsizei depth,
                                         GLint border, GLenum format,
                                         GLenum type, const void* pixels);

struct OpenGlExtraProcs {
    GlUniform1fProc Uniform1f                           = nullptr;
    GlUniform3fProc Uniform3f                           = nullptr;
    GlUniform1fvProc Uniform1fv                         = nullptr;
    GlUniform1ivProc Uniform1iv                         = nullptr;
    GlDrawArraysProc DrawArrays                         = nullptr;
    GlTexImage1DProc TexImage1D                         = nullptr;
    GlGenFramebuffersProc GenFramebuffers               = nullptr;
    GlBindFramebufferProc BindFramebuffer               = nullptr;
    GlDeleteFramebuffersProc DeleteFramebuffers         = nullptr;
    GlFramebufferTexture2DProc FramebufferTexture2D     = nullptr;
    GlCheckFramebufferStatusProc CheckFramebufferStatus = nullptr;
    GlTexImage3DProc TexImage3D                         = nullptr;
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

    RendererBackendState* backend_state(RendererState& renderer_state)
    {
        return reinterpret_cast<RendererBackendState*>(renderer_state.backend);
    }

    bool ensure_basic_preview_program(RendererBackendState& state,
                                      std::string& error_message);
    bool ensure_preview_framebuffer(RendererBackendState& state,
                                    std::string& error_message);

    const RendererTextureBackendState*
    texture_backend_state(const RendererTexture& texture)
    {
        return reinterpret_cast<const RendererTextureBackendState*>(
            texture.backend);
    }

    RendererTextureBackendState* texture_backend_state(RendererTexture& texture)
    {
        return reinterpret_cast<RendererTextureBackendState*>(texture.backend);
    }

    bool ensure_backend_state(RendererState& renderer_state)
    {
        if (renderer_state.backend != nullptr)
            return true;
        renderer_state.backend
            = reinterpret_cast<::Imiv::RendererBackendState*>(
                new RendererBackendState());
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
               && a.use_ocio == b.use_ocio && a.orientation == b.orientation;
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
               && load_gl_proc("glUniform3f", state.extra_procs.Uniform3f,
                               error_message)
               && load_gl_proc("glUniform1fv", state.extra_procs.Uniform1fv,
                               error_message)
               && load_gl_proc("glUniform1iv", state.extra_procs.Uniform1iv,
                               error_message)
               && load_gl_proc("glDrawArrays", state.extra_procs.DrawArrays,
                               error_message)
               && load_gl_proc("glTexImage1D", state.extra_procs.TexImage1D,
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
               && load_gl_proc("glTexImage3D", state.extra_procs.TexImage3D,
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

    void destroy_ocio_preview_resources(OcioPreviewProgram& program)
    {
        for (const OcioPreviewProgram::TextureDesc& texture :
             program.textures) {
            if (texture.texture_id != 0)
                glDeleteTextures(1, &texture.texture_id);
        }
        program.textures.clear();
        program.uniforms.clear();
        if (program.program != 0) {
            glDeleteProgram(program.program);
            program.program = 0;
        }
        program.source_sampler_location = -1;
        program.input_channels_location = -1;
        program.offset_location         = -1;
        program.color_mode_location     = -1;
        program.channel_location        = -1;
        program.orientation_location    = -1;
        program.shader_cache_id.clear();
        program.ready = false;
    }

    void destroy_ocio_preview_program(OcioPreviewProgram& program)
    {
        destroy_ocio_preview_resources(program);
        destroy_ocio_shader_runtime(program.runtime);
    }

    GLenum gl_filter_for_ocio(OcioInterpolation interpolation)
    {
        return interpolation == OcioInterpolation::Nearest ? GL_NEAREST
                                                           : GL_LINEAR;
    }

    GLenum gl_target_for_ocio(OcioTextureDimensions dimensions)
    {
        switch (dimensions) {
        case OcioTextureDimensions::Tex1D: return GL_TEXTURE_1D;
        case OcioTextureDimensions::Tex3D: return GL_TEXTURE_3D;
        case OcioTextureDimensions::Tex2D:
        default: return GL_TEXTURE_2D;
        }
    }

    void set_ocio_texture_parameters(GLenum target, GLenum filter)
    {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        if (target != GL_TEXTURE_1D)
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (target == GL_TEXTURE_3D)
            glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    bool upload_ocio_texture(RendererBackendState& state,
                             const OcioTextureBlueprint& blueprint,
                             GLint texture_unit, GLuint program,
                             OcioPreviewProgram::TextureDesc& texture_desc,
                             std::string& error_message)
    {
        const GLenum target = gl_target_for_ocio(blueprint.dimensions);
        const GLenum filter = gl_filter_for_ocio(blueprint.interpolation);
        const GLenum format = blueprint.channel == OcioTextureChannel::Red
                                  ? GL_RED
                                  : GL_RGB;
        const GLint internal_format = blueprint.channel
                                              == OcioTextureChannel::Red
                                          ? GL_R32F
                                          : GL_RGB32F;

        GLuint texture_id = 0;
        glGenTextures(1, &texture_id);
        if (texture_id == 0) {
            error_message = "failed to create OpenGL OCIO LUT texture";
            return false;
        }

        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(target, texture_id);
        set_ocio_texture_parameters(target, filter);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        const float* values = blueprint.values.empty()
                                  ? nullptr
                                  : blueprint.values.data();
        if (values == nullptr) {
            glDeleteTextures(1, &texture_id);
            error_message = "missing OCIO LUT values";
            return false;
        }

        if (target == GL_TEXTURE_3D) {
            state.extra_procs.TexImage3D(target, 0, internal_format,
                                         static_cast<GLsizei>(blueprint.width),
                                         static_cast<GLsizei>(blueprint.height),
                                         static_cast<GLsizei>(blueprint.depth),
                                         0, format, GL_FLOAT, values);
        } else if (target == GL_TEXTURE_2D) {
            glTexImage2D(target, 0, internal_format,
                         static_cast<GLsizei>(blueprint.width),
                         static_cast<GLsizei>(blueprint.height), 0, format,
                         GL_FLOAT, values);
        } else {
            state.extra_procs.TexImage1D(target, 0, internal_format,
                                         static_cast<GLsizei>(blueprint.width),
                                         0, format, GL_FLOAT, values);
        }

        const GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            glDeleteTextures(1, &texture_id);
            error_message = "OpenGL OCIO texture upload failed";
            return false;
        }

        const GLint sampler_location
            = glGetUniformLocation(program, blueprint.sampler_name.c_str());
        if (sampler_location < 0) {
            glDeleteTextures(1, &texture_id);
            error_message = "missing OpenGL OCIO sampler uniform: "
                            + blueprint.sampler_name;
            return false;
        }

        texture_desc.texture_id       = texture_id;
        texture_desc.target           = target;
        texture_desc.sampler_location = sampler_location;
        texture_desc.texture_unit     = texture_unit;
        return true;
    }

    bool set_ocio_uniform(GLint location,
                          const OCIO::GpuShaderDesc::UniformData& data,
                          OpenGlExtraProcs& extra_procs,
                          std::string& error_message)
    {
        if (location < 0)
            return true;

        switch (data.m_type) {
        case OCIO::UNIFORM_DOUBLE:
            extra_procs.Uniform1f(location,
                                  data.m_getDouble
                                      ? static_cast<float>(data.m_getDouble())
                                      : 0.0f);
            return true;
        case OCIO::UNIFORM_BOOL: {
            const GLint value = data.m_getBool && data.m_getBool() ? 1 : 0;
            glUniform1i(location, value);
            return true;
        }
        case OCIO::UNIFORM_FLOAT3:
            if (data.m_getFloat3) {
                const OCIO::Float3& value = data.m_getFloat3();
                extra_procs.Uniform3f(location, static_cast<float>(value[0]),
                                      static_cast<float>(value[1]),
                                      static_cast<float>(value[2]));
            } else {
                extra_procs.Uniform3f(location, 0.0f, 0.0f, 0.0f);
            }
            return true;
        case OCIO::UNIFORM_VECTOR_FLOAT:
            if (data.m_vectorFloat.m_getSize
                && data.m_vectorFloat.m_getVector) {
                extra_procs.Uniform1fv(location,
                                       static_cast<GLsizei>(
                                           data.m_vectorFloat.m_getSize()),
                                       data.m_vectorFloat.m_getVector());
            }
            return true;
        case OCIO::UNIFORM_VECTOR_INT:
            if (data.m_vectorInt.m_getSize && data.m_vectorInt.m_getVector) {
                extra_procs.Uniform1iv(location,
                                       static_cast<GLsizei>(
                                           data.m_vectorInt.m_getSize()),
                                       data.m_vectorInt.m_getVector());
            }
            return true;
        case OCIO::UNIFORM_UNKNOWN:
        default:
            error_message = "unsupported OpenGL OCIO uniform type";
            return false;
        }
    }

    bool build_ocio_fragment_source(const OcioShaderBlueprint& blueprint,
                                    std::string& shader_source,
                                    std::string& error_message)
    {
        shader_source.clear();
        error_message.clear();
        if (!blueprint.enabled || !blueprint.valid
            || blueprint.shader_text.empty()) {
            error_message = "OpenGL OCIO shader blueprint is invalid";
            return false;
        }

        shader_source = OIIO::Strutil::fmt::format(
            R"glsl(
in vec2 uv_in;
out vec4 out_color;

uniform sampler2D u_source_image;
uniform int u_input_channels;
uniform float u_offset;
uniform int u_color_mode;
uniform int u_channel;
uniform int u_orientation;

vec2 display_to_source_uv(vec2 uv, int orientation)
{{
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
}}

float selected_channel(vec4 c, int channel)
{{
    if (channel == 1)
        return c.r;
    if (channel == 2)
        return c.g;
    if (channel == 3)
        return c.b;
    if (channel == 4)
        return c.a;
    return c.r;
}}

vec3 heatmap(float x)
{{
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
}}

{}

void main()
{{
    vec2 src_uv = display_to_source_uv(uv_in, u_orientation);
    vec4 c = texture(u_source_image, src_uv);
    if (u_input_channels == 2)
        c = vec4(c.rrr, c.g);
    c.rgb += vec3(u_offset);

    if (u_color_mode == 1) {{
        c.a = 1.0;
    }} else if (u_color_mode == 2) {{
        float v = selected_channel(c, u_channel);
        c = vec4(v, v, v, 1.0);
    }} else if (u_color_mode == 3) {{
        float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
        c = vec4(y, y, y, 1.0);
    }} else if (u_color_mode == 4) {{
        float v = selected_channel(c, u_channel);
        c = vec4(heatmap(v), 1.0);
    }}

    if (u_channel > 0 && u_color_mode != 2 && u_color_mode != 4) {{
        float v = selected_channel(c, u_channel);
        c = vec4(v, v, v, 1.0);
    }}

    if (u_input_channels == 1 && u_color_mode <= 1)
        c = vec4(c.rrr, 1.0);
    else if (u_input_channels == 2 && u_color_mode == 0)
        c = vec4(c.rrr, c.a);
    else if (u_input_channels == 2 && u_color_mode == 1)
        c = vec4(c.rrr, 1.0);

    out_color = {}(c);
}}
)glsl",
            blueprint.shader_text, blueprint.function_name);
        return true;
    }

    bool ensure_ocio_preview_program(RendererBackendState& state,
                                     const PlaceholderUiState& ui_state,
                                     const LoadedImage* image,
                                     std::string& error_message)
    {
        if (!ensure_basic_preview_program(state, error_message)
            || !ensure_extra_procs(state, error_message)) {
            return false;
        }
        if (!ensure_ocio_shader_runtime_glsl(ui_state, image,
                                             state.ocio_preview.runtime,
                                             error_message)) {
            return false;
        }
        if (state.ocio_preview.runtime == nullptr
            || state.ocio_preview.runtime->shader_desc == nullptr) {
            error_message = "OpenGL OCIO runtime is not initialized";
            return false;
        }

        const OcioShaderBlueprint& blueprint
            = state.ocio_preview.runtime->blueprint;
        if (state.ocio_preview.ready
            && state.ocio_preview.shader_cache_id
                   == blueprint.shader_cache_id) {
            return true;
        }

        destroy_ocio_preview_resources(state.ocio_preview);

        std::string fragment_source;
        if (!build_ocio_fragment_source(blueprint, fragment_source,
                                        error_message)) {
            return false;
        }

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

        GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        if (vertex_shader == 0 || fragment_shader == 0) {
            if (vertex_shader != 0)
                glDeleteShader(vertex_shader);
            if (fragment_shader != 0)
                glDeleteShader(fragment_shader);
            error_message = "failed to create OpenGL OCIO shader objects";
            return false;
        }

        const char* vertex_sources[]   = { state.glsl_version, "\n",
                                           vertex_source };
        const char* fragment_sources[] = { state.glsl_version, "\n",
                                           fragment_source.c_str() };
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
            error_message = "failed to create OpenGL OCIO program";
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

        state.ocio_preview.program = program;
        state.ocio_preview.source_sampler_location
            = glGetUniformLocation(program, "u_source_image");
        state.ocio_preview.input_channels_location
            = glGetUniformLocation(program, "u_input_channels");
        state.ocio_preview.offset_location = glGetUniformLocation(program,
                                                                  "u_offset");
        state.ocio_preview.color_mode_location
            = glGetUniformLocation(program, "u_color_mode");
        state.ocio_preview.channel_location = glGetUniformLocation(program,
                                                                   "u_channel");
        state.ocio_preview.orientation_location
            = glGetUniformLocation(program, "u_orientation");

        const auto& ocio_textures = blueprint.textures;
        state.ocio_preview.textures.reserve(ocio_textures.size());
        for (size_t i = 0; i < ocio_textures.size(); ++i) {
            OcioPreviewProgram::TextureDesc texture_desc;
            if (!upload_ocio_texture(state, ocio_textures[i],
                                     static_cast<GLint>(i + 1), program,
                                     texture_desc, error_message)) {
                destroy_ocio_preview_resources(state.ocio_preview);
                return false;
            }
            state.ocio_preview.textures.push_back(std::move(texture_desc));
        }

        const unsigned num_uniforms
            = state.ocio_preview.runtime->shader_desc->getNumUniforms();
        state.ocio_preview.uniforms.reserve(num_uniforms);
        for (unsigned idx = 0; idx < num_uniforms; ++idx) {
            OCIO::GpuShaderDesc::UniformData data;
            const char* name
                = state.ocio_preview.runtime->shader_desc->getUniform(idx,
                                                                      data);
            OcioPreviewProgram::UniformDesc uniform;
            if (name != nullptr)
                uniform.name = name;
            uniform.data     = data;
            uniform.location = uniform.name.empty()
                                   ? -1
                                   : glGetUniformLocation(program,
                                                          uniform.name.c_str());
            state.ocio_preview.uniforms.push_back(std::move(uniform));
        }

        state.ocio_preview.shader_cache_id = blueprint.shader_cache_id;
        state.ocio_preview.ready           = true;
        error_message.clear();
        return true;
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
    if (u_input_channels == 2)
        c = vec4(c.rrr, c.g);
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

    bool allocate_source_texture_storage(GLuint texture_id, GLint filter,
                                         int width, int height,
                                         const SourceTextureUploadDesc& upload,
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
        glPixelStorei(GL_UNPACK_ROW_LENGTH, upload.unpack_row_length);
        glTexImage2D(GL_TEXTURE_2D, 0, upload.internal_format, width, height, 0,
                     upload.format, upload.type, upload.pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        const GLenum err = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        if (err == GL_NO_ERROR) {
            error_message.clear();
            return true;
        }
        error_message = "OpenGL texture upload failed";
        return false;
    }

    bool allocate_preview_texture_storage(GLuint texture_id, GLint filter,
                                          int width, int height,
                                          std::string& error_message)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                     GL_FLOAT, nullptr);
        const GLenum err = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        if (err == GL_NO_ERROR) {
            error_message.clear();
            return true;
        }
        error_message = "OpenGL texture allocation failed";
        return false;
    }

    bool render_basic_preview_texture(
        RendererBackendState& state, RendererTextureBackendState& texture_state,
        GLuint target_texture, const RendererPreviewControls& controls,
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

    bool render_ocio_preview_texture(RendererBackendState& state,
                                     RendererTextureBackendState& texture_state,
                                     GLuint target_texture,
                                     const RendererPreviewControls& controls,
                                     std::string& error_message)
    {
        if (!ensure_preview_framebuffer(state, error_message)
            || !state.ocio_preview.ready
            || state.ocio_preview.runtime == nullptr
            || state.ocio_preview.runtime->shader_desc == nullptr
            || !state.basic_preview.ready) {
            error_message = "OpenGL OCIO preview state is not initialized";
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

        if (state.ocio_preview.runtime->exposure_property) {
            state.ocio_preview.runtime->exposure_property->setValue(
                static_cast<double>(controls.exposure));
        }
        if (state.ocio_preview.runtime->gamma_property) {
            const double gamma
                = 1.0 / std::max(1.0e-6, static_cast<double>(controls.gamma));
            state.ocio_preview.runtime->gamma_property->setValue(gamma);
        }

        glViewport(0, 0, texture_state.width, texture_state.height);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(state.ocio_preview.program);
        glBindVertexArray(state.basic_preview.fullscreen_triangle_vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_state.source_texture);
        glUniform1i(state.ocio_preview.source_sampler_location, 0);
        glUniform1i(state.ocio_preview.input_channels_location,
                    texture_state.input_channels);
        state.extra_procs.Uniform1f(state.ocio_preview.offset_location,
                                    controls.offset);
        glUniform1i(state.ocio_preview.color_mode_location,
                    controls.color_mode);
        glUniform1i(state.ocio_preview.channel_location, controls.channel);
        glUniform1i(state.ocio_preview.orientation_location,
                    controls.orientation);

        for (const OcioPreviewProgram::TextureDesc& texture :
             state.ocio_preview.textures) {
            glActiveTexture(GL_TEXTURE0 + texture.texture_unit);
            glBindTexture(texture.target, texture.texture_id);
            glUniform1i(texture.sampler_location, texture.texture_unit);
        }
        for (const OcioPreviewProgram::UniformDesc& uniform :
             state.ocio_preview.uniforms) {
            if (!set_ocio_uniform(uniform.location, uniform.data,
                                  state.extra_procs, error_message)) {
                glBindVertexArray(0);
                glUseProgram(0);
                state.extra_procs.BindFramebuffer(GL_FRAMEBUFFER, 0);
                return false;
            }
        }

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
        error_message = "OpenGL OCIO preview draw failed";
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

    bool describe_native_source_upload(const LoadedImage& image,
                                       SourceTextureUploadDesc& upload,
                                       std::string& error_message)
    {
        upload = {};
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
            error_message = "invalid source image dimensions";
            return false;
        }

        const int channel_count = image.nchannels;
        if (channel_count > 4) {
            if (!build_rgba_float_pixels(image, upload.fallback_rgba_data,
                                         error_message)) {
                return false;
            }
            upload.pixels            = upload.fallback_rgba_data.data();
            upload.unpack_row_length = image.width;
            error_message.clear();
            return true;
        }

        const size_t pixel_stride = image.channel_bytes
                                    * static_cast<size_t>(channel_count);
        if (pixel_stride == 0 || image.row_pitch_bytes == 0
            || image.row_pitch_bytes
                   < static_cast<size_t>(image.width) * pixel_stride) {
            error_message = "invalid source stride for OpenGL upload";
            return false;
        }

        if (image.row_pitch_bytes % pixel_stride != 0) {
            if (!build_rgba_float_pixels(image, upload.fallback_rgba_data,
                                         error_message)) {
                return false;
            }
            upload.pixels            = upload.fallback_rgba_data.data();
            upload.unpack_row_length = image.width;
            error_message.clear();
            return true;
        }

        const GLint row_length   = static_cast<GLint>(image.row_pitch_bytes
                                                      / pixel_stride);
        upload.pixels            = image.pixels.data();
        upload.unpack_row_length = row_length;

        switch (image.nchannels) {
        case 1: upload.format = GL_RED; break;
        case 2: upload.format = GL_RG; break;
        case 3: upload.format = GL_RGB; break;
        case 4: upload.format = GL_RGBA; break;
        default: break;
        }

        switch (image.type) {
        case UploadDataType::UInt8:
            upload.type = GL_UNSIGNED_BYTE;
            switch (image.nchannels) {
            case 1: upload.internal_format = GL_R8; break;
            case 2: upload.internal_format = GL_RG8; break;
            case 3: upload.internal_format = GL_RGB8; break;
            case 4: upload.internal_format = GL_RGBA8; break;
            default: break;
            }
            break;
        case UploadDataType::UInt16:
            upload.type = GL_UNSIGNED_SHORT;
            switch (image.nchannels) {
            case 1: upload.internal_format = GL_R16; break;
            case 2: upload.internal_format = GL_RG16; break;
            case 3: upload.internal_format = GL_RGB16; break;
            case 4: upload.internal_format = GL_RGBA16; break;
            default: break;
            }
            break;
        case UploadDataType::Half:
            upload.type = GL_HALF_FLOAT;
            switch (image.nchannels) {
            case 1: upload.internal_format = GL_R16F; break;
            case 2: upload.internal_format = GL_RG16F; break;
            case 3: upload.internal_format = GL_RGB16F; break;
            case 4: upload.internal_format = GL_RGBA16F; break;
            default: break;
            }
            break;
        case UploadDataType::Float:
            upload.type = GL_FLOAT;
            switch (image.nchannels) {
            case 1: upload.internal_format = GL_R32F; break;
            case 2: upload.internal_format = GL_RG32F; break;
            case 3: upload.internal_format = GL_RGB32F; break;
            case 4: upload.internal_format = GL_RGBA32F; break;
            default: break;
            }
            break;
        case UploadDataType::UInt32:
        case UploadDataType::Double:
            if (!build_rgba_float_pixels(image, upload.fallback_rgba_data,
                                         error_message)) {
                return false;
            }
            upload.internal_format   = GL_RGBA32F;
            upload.format            = GL_RGBA;
            upload.type              = GL_FLOAT;
            upload.pixels            = upload.fallback_rgba_data.data();
            upload.unpack_row_length = image.width;
            error_message.clear();
            return true;
        case UploadDataType::Unknown:
        default: error_message = "unsupported source pixel type"; return false;
        }

        error_message.clear();
        return true;
    }

bool
opengl_get_viewer_texture_refs(const ViewerState& viewer,
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
opengl_texture_is_loading(const RendererTexture& texture)
{
    return texture.backend != nullptr && !texture.preview_initialized;
}

bool
opengl_create_texture(RendererState& renderer_state, const LoadedImage& image,
                      RendererTexture& texture, std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr) {
        error_message = "OpenGL window is not initialized";
        return false;
    }

    SourceTextureUploadDesc upload;
    if (!describe_native_source_upload(image, upload, error_message))
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
        || !allocate_source_texture_storage(texture_state->source_texture,
                                            GL_NEAREST, image.width,
                                            image.height, upload, error_message)
        || !allocate_preview_texture_storage(
            texture_state->preview_linear_texture, GL_LINEAR, image.width,
            image.height, error_message)
        || !allocate_preview_texture_storage(
            texture_state->preview_nearest_texture, GL_NEAREST, image.width,
            image.height, error_message)) {
        if (texture_state->source_texture != 0)
            glDeleteTextures(1, &texture_state->source_texture);
        if (texture_state->preview_linear_texture != 0)
            glDeleteTextures(1, &texture_state->preview_linear_texture);
        if (texture_state->preview_nearest_texture != 0)
            glDeleteTextures(1, &texture_state->preview_nearest_texture);
        delete texture_state;
        return false;
    }

    texture.backend             = reinterpret_cast<
        ::Imiv::RendererTextureBackendState*>(texture_state);
    texture.preview_initialized = false;
    error_message.clear();
    return true;
}

void
opengl_destroy_texture(RendererState& renderer_state, RendererTexture& texture)
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
opengl_update_preview_texture(RendererState& renderer_state,
                              RendererTexture& texture,
                              const LoadedImage* image,
                              const PlaceholderUiState& ui_state,
                              const RendererPreviewControls& controls,
                              std::string& error_message)
{
    RendererBackendState* state                = backend_state(renderer_state);
    RendererTextureBackendState* texture_state = texture_backend_state(texture);
    if (state == nullptr || state->window == nullptr
        || texture_state == nullptr) {
        error_message = "OpenGL preview state is not initialized";
        return false;
    }

    RendererPreviewControls effective_controls = controls;
    std::string ocio_shader_cache_id;
    bool use_ocio_preview = false;

    platform_glfw_make_context_current(state->window);
    if (controls.use_ocio != 0) {
        if (ensure_ocio_preview_program(*state, ui_state, image,
                                        error_message)) {
            use_ocio_preview     = true;
            ocio_shader_cache_id = state->ocio_preview.shader_cache_id;
        } else {
            if (!error_message.empty()) {
                std::cerr << "imiv: OpenGL OCIO fallback: " << error_message
                          << "\n";
            }
            effective_controls.use_ocio = 0;
            ocio_shader_cache_id.clear();
            error_message.clear();
        }
    }

    if (!texture_state->preview_dirty && texture_state->preview_params_valid
        && preview_controls_equal(texture_state->last_preview_controls,
                                  effective_controls)
        && texture_state->last_ocio_shader_cache_id == ocio_shader_cache_id) {
        texture.preview_initialized = true;
        error_message.clear();
        return true;
    }

    const bool ok = use_ocio_preview
                        ? render_ocio_preview_texture(
                              *state, *texture_state,
                              texture_state->preview_linear_texture,
                              effective_controls, error_message)
                              && render_ocio_preview_texture(
                                  *state, *texture_state,
                                  texture_state->preview_nearest_texture,
                                  effective_controls, error_message)
                        : render_basic_preview_texture(
                              *state, *texture_state,
                              texture_state->preview_linear_texture,
                              effective_controls, error_message)
                              && render_basic_preview_texture(
                                  *state, *texture_state,
                                  texture_state->preview_nearest_texture,
                                  effective_controls, error_message);
    if (!ok) {
        texture.preview_initialized = false;
        return false;
    }

    texture_state->preview_dirty             = false;
    texture_state->preview_params_valid      = true;
    texture_state->last_preview_controls     = effective_controls;
    texture_state->last_ocio_shader_cache_id = ocio_shader_cache_id;
    texture.preview_initialized              = true;
    error_message.clear();
    return true;
}

bool
opengl_quiesce_texture_preview_submission(RendererState& renderer_state,
                                          RendererTexture& texture,
                                          std::string& error_message)
{
    (void)renderer_state;
    (void)texture;
    error_message.clear();
    return true;
}

bool
opengl_setup_instance(RendererState& renderer_state,
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
opengl_setup_device(RendererState& renderer_state, std::string& error_message)
{
    if (backend_state(renderer_state) == nullptr) {
        error_message = "OpenGL renderer state is not initialized";
        return false;
    }
    error_message.clear();
    return true;
}

bool
opengl_setup_window(RendererState& renderer_state, int width, int height,
                    std::string& error_message)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    error_message.clear();
    return true;
}

bool
opengl_create_surface(RendererState& renderer_state, GLFWwindow* window,
                      std::string& error_message)
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
opengl_destroy_surface(RendererState& renderer_state)
{
    if (RendererBackendState* state = backend_state(renderer_state))
        state->window = nullptr;
}

void
opengl_cleanup_window(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
opengl_cleanup(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state != nullptr) {
        if (state->window != nullptr)
            platform_glfw_make_context_current(state->window);
        destroy_ocio_preview_program(state->ocio_preview);
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
opengl_wait_idle(RendererState& renderer_state, std::string& error_message)
{
    (void)renderer_state;
    error_message.clear();
    return true;
}

bool
opengl_imgui_init(RendererState& renderer_state, std::string& error_message)
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
opengl_imgui_shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
}

void
opengl_imgui_new_frame(RendererState& renderer_state)
{
    (void)renderer_state;
    ImGui_ImplOpenGL3_NewFrame();
}

bool
opengl_needs_main_window_resize(RendererState& renderer_state, int width,
                                int height)
{
    return renderer_state.framebuffer_width != width
           || renderer_state.framebuffer_height != height;
}

void
opengl_resize_main_window(RendererState& renderer_state, int width, int height)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
}

void
opengl_set_main_clear_color(RendererState& renderer_state, float r, float g,
                            float b, float a)
{
    renderer_state.clear_color[0] = r;
    renderer_state.clear_color[1] = g;
    renderer_state.clear_color[2] = b;
    renderer_state.clear_color[3] = a;
}

void
opengl_prepare_platform_windows(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr)
        return;
    state->backup_context = platform_glfw_get_current_context();
}

void
opengl_finish_platform_windows(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->backup_context == nullptr)
        return;
    platform_glfw_make_context_current(state->backup_context);
    state->backup_context = nullptr;
}

void
opengl_frame_render(RendererState& renderer_state, ImDrawData* draw_data)
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
opengl_frame_present(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr)
        return;
    platform_glfw_swap_buffers(state->window);
}

bool
opengl_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                      unsigned int* pixels, void* user_data)
{
    RendererState* renderer_state = reinterpret_cast<RendererState*>(user_data);
    if (renderer_state == nullptr || pixels == nullptr || w <= 0 || h <= 0)
        return false;

    RendererBackendState* state = backend_state(*renderer_state);
    if (state == nullptr || state->window == nullptr)
        return false;

    int framebuffer_width  = 0;
    int framebuffer_height = 0;
    platform_glfw_make_context_current(state->window);
    platform_glfw_get_framebuffer_size(state->window, framebuffer_width,
                                       framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
        return false;

    int capture_x = x;
    int capture_y = y;
    int capture_w = w;
    int capture_h = h;
    ImGuiViewport* viewport = ImGui::FindViewportByID(viewport_id);
    if (viewport != nullptr && viewport->Size.x > 0.0f
        && viewport->Size.y > 0.0f) {
        const double scale_x = static_cast<double>(framebuffer_width)
                               / static_cast<double>(viewport->Size.x);
        const double scale_y = static_cast<double>(framebuffer_height)
                               / static_cast<double>(viewport->Size.y);
        capture_x = static_cast<int>(std::lround(
            (static_cast<double>(x) - static_cast<double>(viewport->Pos.x))
            * scale_x));
        capture_y = static_cast<int>(std::lround(
            (static_cast<double>(y) - static_cast<double>(viewport->Pos.y))
            * scale_y));
        capture_w = std::max(1, static_cast<int>(std::lround(
                                   static_cast<double>(w) * scale_x)));
        capture_h = std::max(1, static_cast<int>(std::lround(
                                   static_cast<double>(h) * scale_y)));
    }

    if (capture_x < 0) {
        capture_w += capture_x;
        capture_x = 0;
    }
    if (capture_y < 0) {
        capture_h += capture_y;
        capture_y = 0;
    }
    if (capture_x < framebuffer_width && capture_y < framebuffer_height) {
        capture_w = std::min(capture_w, framebuffer_width - capture_x);
        capture_h = std::min(capture_h, framebuffer_height - capture_y);
    }
    if (capture_w <= 0 || capture_h <= 0)
        return false;

    const int read_y = framebuffer_height - (capture_y + capture_h);
    if (read_y < 0)
        return false;

    std::vector<unsigned char> readback(static_cast<size_t>(capture_w)
                                        * static_cast<size_t>(capture_h) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(capture_x, read_y, capture_w, capture_h, GL_RGBA,
                 GL_UNSIGNED_BYTE, readback.data());
    if (glGetError() != GL_NO_ERROR)
        return false;

    unsigned char* dst_bytes = reinterpret_cast<unsigned char*>(pixels);
    if (capture_w == w && capture_h == h) {
        for (int row = 0; row < h; ++row) {
            const size_t src_offset = static_cast<size_t>(h - 1 - row)
                                      * static_cast<size_t>(w) * 4;
            const size_t dst_offset = static_cast<size_t>(row)
                                      * static_cast<size_t>(w) * 4;
            std::memcpy(dst_bytes + dst_offset, readback.data() + src_offset,
                        static_cast<size_t>(w) * 4);
        }
        return true;
    }

    const double sample_scale_x = static_cast<double>(capture_w)
                                  / static_cast<double>(w);
    const double sample_scale_y = static_cast<double>(capture_h)
                                  / static_cast<double>(h);
    for (int row = 0; row < h; ++row) {
        unsigned char* dst_row = dst_bytes
                                 + static_cast<size_t>(row)
                                       * static_cast<size_t>(w) * 4;
        const int sample_row = std::clamp(
            static_cast<int>(std::floor((static_cast<double>(row) + 0.5)
                                        * sample_scale_y)),
            0, capture_h - 1);
        const unsigned char* src_row
            = readback.data()
              + static_cast<size_t>(capture_h - 1 - sample_row)
                    * static_cast<size_t>(capture_w) * 4;
        for (int col = 0; col < w; ++col) {
            const int sample_col = std::clamp(
                static_cast<int>(std::floor((static_cast<double>(col) + 0.5)
                                            * sample_scale_x)),
                0, capture_w - 1);
            const unsigned char* src
                = src_row + static_cast<size_t>(sample_col) * 4;
            unsigned char* dst = dst_row + static_cast<size_t>(col) * 4;
            dst[0]             = src[0];
            dst[1]             = src[1];
            dst[2]             = src[2];
            dst[3]             = src[3];
        }
    }
    return true;
}

const RendererBackendVTable k_opengl_vtable = {
    BackendKind::OpenGL,
    opengl_get_viewer_texture_refs,
    opengl_texture_is_loading,
    opengl_create_texture,
    opengl_destroy_texture,
    opengl_update_preview_texture,
    opengl_quiesce_texture_preview_submission,
    opengl_setup_instance,
    opengl_setup_device,
    opengl_setup_window,
    opengl_create_surface,
    opengl_destroy_surface,
    opengl_cleanup_window,
    opengl_cleanup,
    opengl_wait_idle,
    opengl_imgui_init,
    opengl_imgui_shutdown,
    opengl_imgui_new_frame,
    opengl_needs_main_window_resize,
    opengl_resize_main_window,
    opengl_set_main_clear_color,
    opengl_prepare_platform_windows,
    opengl_finish_platform_windows,
    opengl_frame_render,
    opengl_frame_present,
    opengl_screen_capture,
};

}  // namespace
}  // namespace Imiv

namespace Imiv {

const RendererBackendVTable*
renderer_backend_opengl_vtable()
{
    return &k_opengl_vtable;
}

}  // namespace Imiv
