// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_platform_glfw.h"
#include "imiv_viewer.h"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_opengl3_loader.h>

#include <OpenImageIO/imagebuf.h>

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

namespace Imiv {

struct RendererTextureBackendState {
    GLuint linear_texture  = 0;
    GLuint nearest_texture = 0;
    int width              = 0;
    int height             = 0;
};

struct RendererBackendState {
    GLFWwindow* window         = nullptr;
    GLFWwindow* backup_context = nullptr;
    const char* glsl_version   = nullptr;
    bool imgui_initialized     = false;
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

    bool make_texture_2d(GLuint texture_id, GLint min_mag_filter, int width,
                         int height, const float* rgba_pixels,
                         std::string& error_message)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_mag_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, min_mag_filter);
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
            if (channels >= 1)
                dst[0] = source_pixels[src + 0];
            if (channels >= 2)
                dst[1] = source_pixels[src + 1];
            else
                dst[1] = dst[0];
            if (channels >= 3)
                dst[2] = source_pixels[src + 2];
            else
                dst[2] = dst[1];
            if (channels >= 4)
                dst[3] = source_pixels[src + 3];
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
                                    ? state->linear_texture
                                    : state->nearest_texture;
    if (main_texture != 0) {
        main_texture_ref = ImTextureRef(
            static_cast<ImTextureID>(static_cast<intptr_t>(main_texture)));
        has_main_texture = true;
    }

    const GLuint closeup_texture = state->nearest_texture != 0
                                       ? state->nearest_texture
                                       : state->linear_texture;
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
    glGenTextures(1, &texture_state->linear_texture);
    glGenTextures(1, &texture_state->nearest_texture);
    texture_state->width  = image.width;
    texture_state->height = image.height;
    if (texture_state->linear_texture == 0
        || texture_state->nearest_texture == 0
        || !make_texture_2d(texture_state->linear_texture, GL_LINEAR,
                            image.width, image.height, rgba_pixels.data(),
                            error_message)
        || !make_texture_2d(texture_state->nearest_texture, GL_NEAREST,
                            image.width, image.height, rgba_pixels.data(),
                            error_message)) {
        if (texture_state->linear_texture != 0)
            glDeleteTextures(1, &texture_state->linear_texture);
        if (texture_state->nearest_texture != 0)
            glDeleteTextures(1, &texture_state->nearest_texture);
        delete texture_state;
        return false;
    }

    texture.backend             = texture_state;
    texture.preview_initialized = true;
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
    if (state->linear_texture != 0)
        glDeleteTextures(1, &state->linear_texture);
    if (state->nearest_texture != 0)
        glDeleteTextures(1, &state->nearest_texture);
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
    (void)renderer_state;
    (void)texture;
    (void)image;
    (void)ui_state;
    (void)controls;
    error_message.clear();
    texture.preview_initialized = (texture.backend != nullptr);
    return texture.preview_initialized;
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
    delete backend_state(renderer_state);
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
