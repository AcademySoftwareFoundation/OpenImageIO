// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_platform_glfw.h"
#include "imiv_viewer.h"

#include <imgui_impl_metal.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <OpenImageIO/imagebuf.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Imiv {

struct RendererTextureBackendState {
    __strong id<MTLTexture> preview_linear_texture  = nil;
    __strong id<MTLTexture> preview_nearest_texture = nil;
    int width                                       = 0;
    int height                                      = 0;
    int input_channels                              = 0;
    bool preview_dirty                              = true;
    bool preview_params_valid                       = false;
    RendererPreviewControls last_preview_controls   = {};
    std::vector<float> source_rgba_pixels;
    std::vector<float> preview_rgba_pixels;
};

struct RendererBackendState {
    GLFWwindow* window                           = nullptr;
    id<MTLDevice> device                         = nil;
    id<MTLCommandQueue> command_queue            = nil;
    CAMetalLayer* layer                          = nil;
    MTLRenderPassDescriptor* render_pass         = nil;
    id<CAMetalDrawable> current_drawable         = nil;
    id<MTLCommandBuffer> current_command_buffer  = nil;
    id<MTLRenderCommandEncoder> current_encoder  = nil;
    bool imgui_initialized                       = false;
};

namespace {

RendererBackendState*
backend_state(RendererState& renderer_state)
{
    return static_cast<RendererBackendState*>(renderer_state.backend);
}

const RendererTextureBackendState*
texture_backend_state(const RendererTexture& texture)
{
    return static_cast<const RendererTextureBackendState*>(texture.backend);
}

RendererTextureBackendState*
texture_backend_state(RendererTexture& texture)
{
    return static_cast<RendererTextureBackendState*>(texture.backend);
}

bool
ensure_backend_state(RendererState& renderer_state)
{
    if (renderer_state.backend != nullptr)
        return true;
    renderer_state.backend = new RendererBackendState();
    return renderer_state.backend != nullptr;
}

void
update_drawable_size(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr || state->layer == nil)
        return;
    int width  = 0;
    int height = 0;
    platform_glfw_get_framebuffer_size(state->window, width, height);
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    state->layer.drawableSize         = CGSizeMake(width, height);
}

bool
preview_controls_equal(const RendererPreviewControls& a,
                       const RendererPreviewControls& b)
{
    return std::abs(a.exposure - b.exposure) < 1.0e-6f
           && std::abs(a.gamma - b.gamma) < 1.0e-6f
           && std::abs(a.offset - b.offset) < 1.0e-6f
           && a.color_mode == b.color_mode && a.channel == b.channel
           && a.use_ocio == b.use_ocio && a.orientation == b.orientation;
}

bool
build_rgba_float_pixels(const LoadedImage& image,
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
        error_message = source.geterror().empty() ? "failed to stage image data"
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

inline float
selected_channel(const float* rgba, int channel)
{
    if (channel == 1)
        return rgba[0];
    if (channel == 2)
        return rgba[1];
    if (channel == 3)
        return rgba[2];
    if (channel == 4)
        return rgba[3];
    return rgba[0];
}

void
heatmap(float x, float* rgb)
{
    const float t = std::clamp(x, 0.0f, 1.0f);
    const float a[3] = { 0.0f, 0.0f, 0.5f };
    const float b[3] = { 0.0f, 0.9f, 1.0f };
    const float c[3] = { 1.0f, 1.0f, 0.0f };
    const float d[3] = { 1.0f, 0.0f, 0.0f };
    const float* lhs = a;
    const float* rhs = b;
    float u          = 0.0f;
    if (t < 0.33f) {
        lhs = a;
        rhs = b;
        u   = t / 0.33f;
    } else if (t < 0.66f) {
        lhs = b;
        rhs = c;
        u   = (t - 0.33f) / 0.33f;
    } else {
        lhs = c;
        rhs = d;
        u   = (t - 0.66f) / 0.34f;
    }
    rgb[0] = lhs[0] + (rhs[0] - lhs[0]) * u;
    rgb[1] = lhs[1] + (rhs[1] - lhs[1]) * u;
    rgb[2] = lhs[2] + (rhs[2] - lhs[2]) * u;
}

inline void
display_to_source_uv(float u, float v, int orientation, float& src_u,
                     float& src_v)
{
    if (orientation == 2) {
        src_u = 1.0f - u;
        src_v = v;
    } else if (orientation == 3) {
        src_u = 1.0f - u;
        src_v = 1.0f - v;
    } else if (orientation == 4) {
        src_u = u;
        src_v = 1.0f - v;
    } else if (orientation == 5) {
        src_u = v;
        src_v = u;
    } else if (orientation == 6) {
        src_u = v;
        src_v = 1.0f - u;
    } else if (orientation == 7) {
        src_u = 1.0f - v;
        src_v = 1.0f - u;
    } else if (orientation == 8) {
        src_u = 1.0f - v;
        src_v = u;
    } else {
        src_u = u;
        src_v = v;
    }
}

inline const float*
sample_source_pixel(const RendererTextureBackendState& state, float u, float v)
{
    const int sx = std::clamp(static_cast<int>(u * state.width), 0,
                              state.width - 1);
    const int sy = std::clamp(static_cast<int>(v * state.height), 0,
                              state.height - 1);
    const size_t offset = (static_cast<size_t>(sy) * state.width
                           + static_cast<size_t>(sx))
                          * 4;
    return state.source_rgba_pixels.data() + offset;
}

void
generate_preview_pixels(const RendererTextureBackendState& state,
                        const RendererPreviewControls& controls,
                        std::vector<float>& preview_pixels)
{
    preview_pixels.assign(static_cast<size_t>(state.width) * state.height * 4,
                          1.0f);
    const float exposure_scale = std::exp2(controls.exposure);
    const float gamma          = std::max(controls.gamma, 0.01f);

    for (int y = 0; y < state.height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f)
                        / static_cast<float>(state.height);
        for (int x = 0; x < state.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f)
                            / static_cast<float>(state.width);
            float src_u = u;
            float src_v = v;
            display_to_source_uv(u, v, controls.orientation, src_u, src_v);

            const float* src = sample_source_pixel(state, src_u, src_v);
            float rgba[4]    = { src[0], src[1], src[2], src[3] };
            rgba[0] += controls.offset;
            rgba[1] += controls.offset;
            rgba[2] += controls.offset;

            if (controls.color_mode == 1) {
                rgba[3] = 1.0f;
            } else if (controls.color_mode == 2) {
                const float value = selected_channel(rgba, controls.channel);
                rgba[0]           = value;
                rgba[1]           = value;
                rgba[2]           = value;
                rgba[3]           = 1.0f;
            } else if (controls.color_mode == 3) {
                const float value = rgba[0] * 0.2126f + rgba[1] * 0.7152f
                                    + rgba[2] * 0.0722f;
                rgba[0] = value;
                rgba[1] = value;
                rgba[2] = value;
                rgba[3] = 1.0f;
            } else if (controls.color_mode == 4) {
                const float value = selected_channel(rgba, controls.channel);
                heatmap(value, rgba);
                rgba[3] = 1.0f;
            }

            if (controls.channel > 0 && controls.color_mode != 2
                && controls.color_mode != 4) {
                const float value = selected_channel(rgba, controls.channel);
                rgba[0]           = value;
                rgba[1]           = value;
                rgba[2]           = value;
                rgba[3]           = 1.0f;
            }

            if (state.input_channels == 1 && controls.color_mode <= 1) {
                rgba[1] = rgba[0];
                rgba[2] = rgba[0];
                rgba[3] = 1.0f;
            } else if (state.input_channels == 2 && controls.color_mode == 0) {
                rgba[1] = rgba[0];
                rgba[2] = rgba[0];
            } else if (state.input_channels == 2 && controls.color_mode == 1) {
                rgba[1] = rgba[0];
                rgba[2] = rgba[0];
                rgba[3] = 1.0f;
            }

            rgba[0] = std::pow(std::max(rgba[0] * exposure_scale, 0.0f),
                               1.0f / gamma);
            rgba[1] = std::pow(std::max(rgba[1] * exposure_scale, 0.0f),
                               1.0f / gamma);
            rgba[2] = std::pow(std::max(rgba[2] * exposure_scale, 0.0f),
                               1.0f / gamma);

            const size_t dst_offset = (static_cast<size_t>(y) * state.width
                                       + static_cast<size_t>(x))
                                      * 4;
            preview_pixels[dst_offset + 0] = rgba[0];
            preview_pixels[dst_offset + 1] = rgba[1];
            preview_pixels[dst_offset + 2] = rgba[2];
            preview_pixels[dst_offset + 3] = rgba[3];
        }
    }
}

bool
create_preview_texture(id<MTLDevice> device, int width, int height,
                       id<MTLTexture>& texture, std::string& error_message)
{
    texture = nil;
    if (device == nil || width <= 0 || height <= 0) {
        error_message = "invalid Metal preview texture parameters";
        return false;
    }
    MTLTextureDescriptor* descriptor
        = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    texture          = [device newTextureWithDescriptor:descriptor];
    if (texture == nil) {
        error_message = "failed to create Metal preview texture";
        return false;
    }
    error_message.clear();
    return true;
}

bool
upload_preview_pixels(id<MTLTexture> texture, int width, int height,
                      const std::vector<float>& preview_pixels,
                      std::string& error_message)
{
    if (texture == nil || width <= 0 || height <= 0) {
        error_message = "invalid Metal texture upload state";
        return false;
    }
    const size_t expected = static_cast<size_t>(width) * height * 4;
    if (preview_pixels.size() != expected) {
        error_message = "invalid Metal preview pixel buffer size";
        return false;
    }
    const MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(width),
                                             static_cast<NSUInteger>(height));
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:preview_pixels.data()
               bytesPerRow:static_cast<NSUInteger>(width * 4 * sizeof(float))];
    error_message.clear();
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

    id<MTLTexture> main_texture = ui_state.linear_interpolation != 0
                                      ? state->preview_linear_texture
                                      : state->preview_nearest_texture;
    if (main_texture == nil)
        main_texture = state->preview_linear_texture;
    if (main_texture != nil) {
        main_texture_ref = ImTextureRef(static_cast<ImTextureID>(
            reinterpret_cast<uintptr_t>((__bridge void*)main_texture)));
        has_main_texture = true;
    }

    id<MTLTexture> closeup_texture = state->preview_nearest_texture;
    if (closeup_texture == nil)
        closeup_texture = state->preview_linear_texture;
    if (closeup_texture != nil) {
        closeup_texture_ref = ImTextureRef(static_cast<ImTextureID>(
            reinterpret_cast<uintptr_t>((__bridge void*)closeup_texture)));
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
    if (state == nullptr || state->device == nil) {
        error_message = "Metal window/device is not initialized";
        return false;
    }

    auto* texture_state = new RendererTextureBackendState();
    if (texture_state == nullptr) {
        error_message = "failed to allocate Metal texture state";
        return false;
    }

    if (!build_rgba_float_pixels(image, texture_state->source_rgba_pixels,
                                 error_message)
        || !create_preview_texture(state->device, image.width, image.height,
                                   texture_state->preview_linear_texture,
                                   error_message)
        || !create_preview_texture(state->device, image.width, image.height,
                                   texture_state->preview_nearest_texture,
                                   error_message)) {
        texture_state->preview_linear_texture  = nil;
        texture_state->preview_nearest_texture = nil;
        delete texture_state;
        return false;
    }

    texture_state->width          = image.width;
    texture_state->height         = image.height;
    texture_state->input_channels = image.nchannels;
    texture_state->preview_dirty  = true;

    texture.backend             = texture_state;
    texture.preview_initialized = false;
    error_message.clear();
    return true;
}

void
renderer_backend_destroy_texture(RendererState& renderer_state,
                                 RendererTexture& texture)
{
    (void)renderer_state;
    RendererTextureBackendState* state = texture_backend_state(texture);
    if (state == nullptr) {
        texture.preview_initialized = false;
        return;
    }
    state->preview_linear_texture  = nil;
    state->preview_nearest_texture = nil;
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
    (void)image;
    (void)ui_state;

    RendererTextureBackendState* texture_state = texture_backend_state(texture);
    if (texture_state == nullptr) {
        error_message = "Metal preview state is not initialized";
        return false;
    }

    RendererPreviewControls effective_controls = controls;
    effective_controls.use_ocio                = 0;
    if (!texture_state->preview_dirty && texture_state->preview_params_valid
        && preview_controls_equal(texture_state->last_preview_controls,
                                  effective_controls)) {
        texture.preview_initialized = true;
        error_message.clear();
        return true;
    }

    generate_preview_pixels(*texture_state, effective_controls,
                            texture_state->preview_rgba_pixels);
    if (!upload_preview_pixels(texture_state->preview_linear_texture,
                               texture_state->width, texture_state->height,
                               texture_state->preview_rgba_pixels,
                               error_message)
        || !upload_preview_pixels(texture_state->preview_nearest_texture,
                                  texture_state->width, texture_state->height,
                                  texture_state->preview_rgba_pixels,
                                  error_message)) {
        texture.preview_initialized = false;
        return false;
    }

    texture_state->preview_dirty         = false;
    texture_state->preview_params_valid  = true;
    texture_state->last_preview_controls = effective_controls;
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
        error_message = "failed to allocate Metal renderer state";
        return false;
    }
    error_message.clear();
    return true;
}

bool
renderer_backend_create_surface(RendererState& renderer_state,
                                GLFWwindow* window,
                                std::string& error_message)
{
    if (!ensure_backend_state(renderer_state)) {
        error_message = "failed to allocate Metal renderer state";
        return false;
    }
    backend_state(renderer_state)->window = window;
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_device(RendererState& renderer_state,
                              std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr) {
        error_message = "Metal renderer state is not initialized";
        return false;
    }
    state->device = MTLCreateSystemDefaultDevice();
    if (state->device == nil) {
        error_message = "MTLCreateSystemDefaultDevice failed";
        return false;
    }
    state->command_queue = [state->device newCommandQueue];
    if (state->command_queue == nil) {
        error_message = "failed to create Metal command queue";
        return false;
    }
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_window(RendererState& renderer_state, int width,
                              int height, std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr || state->device == nil) {
        error_message = "Metal window/device is not initialized";
        return false;
    }
    NSWindow* ns_window = glfwGetCocoaWindow(state->window);
    if (ns_window == nil) {
        error_message = "failed to get Cocoa window from GLFW";
        return false;
    }
    state->layer                  = [CAMetalLayer layer];
    state->layer.device           = state->device;
    state->layer.pixelFormat      = MTLPixelFormatBGRA8Unorm;
    state->layer.framebufferOnly  = YES;
    ns_window.contentView.layer   = state->layer;
    ns_window.contentView.wantsLayer = YES;
    state->render_pass            = [MTLRenderPassDescriptor new];
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    update_drawable_size(renderer_state);
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
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr)
        return;
    state->current_drawable       = nil;
    state->current_command_buffer = nil;
    state->current_encoder        = nil;
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
    RendererBackendState* state = backend_state(renderer_state);
    if (state != nullptr && state->current_command_buffer != nil)
        [state->current_command_buffer waitUntilCompleted];
    error_message.clear();
    return true;
}

bool
renderer_backend_imgui_init(RendererState& renderer_state,
                            std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->device == nil) {
        error_message = "Metal device is not initialized";
        return false;
    }
    ImGui_ImplMetal_Init(state->device);
    state->imgui_initialized = true;
    error_message.clear();
    return true;
}

void
renderer_backend_imgui_shutdown()
{
    ImGui_ImplMetal_Shutdown();
}

void
renderer_backend_imgui_new_frame(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->layer == nil || state->render_pass == nil)
        return;
    update_drawable_size(renderer_state);
    state->current_drawable = [state->layer nextDrawable];
    if (state->current_drawable == nil)
        return;
    state->render_pass.colorAttachments[0].texture
        = state->current_drawable.texture;
    state->render_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    state->render_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    state->render_pass.colorAttachments[0].clearColor = MTLClearColorMake(
        renderer_state.clear_color[0] * renderer_state.clear_color[3],
        renderer_state.clear_color[1] * renderer_state.clear_color[3],
        renderer_state.clear_color[2] * renderer_state.clear_color[3],
        renderer_state.clear_color[3]);
    ImGui_ImplMetal_NewFrame(state->render_pass);
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
    update_drawable_size(renderer_state);
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
    (void)renderer_state;
}

void
renderer_backend_finish_platform_windows(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_frame_render(RendererState& renderer_state,
                              ImDrawData* draw_data)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->current_drawable == nil
        || state->command_queue == nil || state->render_pass == nil) {
        return;
    }
    state->current_command_buffer = [state->command_queue commandBuffer];
    state->current_encoder = [state->current_command_buffer
        renderCommandEncoderWithDescriptor:state->render_pass];
    ImGui_ImplMetal_RenderDrawData(draw_data, state->current_command_buffer,
                                   state->current_encoder);
}

void
renderer_backend_frame_present(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->current_command_buffer == nil
        || state->current_encoder == nil || state->current_drawable == nil) {
        return;
    }
    [state->current_encoder endEncoding];
    [state->current_command_buffer presentDrawable:state->current_drawable];
    [state->current_command_buffer commit];
    state->current_encoder        = nil;
    state->current_command_buffer = nil;
    state->current_drawable       = nil;
}

bool
renderer_backend_screen_capture(ImGuiID viewport_id, int x, int y, int w,
                                int h, unsigned int* pixels, void* user_data)
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
