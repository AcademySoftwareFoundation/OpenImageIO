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
#include <cstring>
#include <string>
#include <vector>

namespace Imiv {

struct RendererTextureBackendState {
    __strong id<MTLTexture> source_texture          = nil;
    __strong id<MTLTexture> preview_linear_texture  = nil;
    __strong id<MTLTexture> preview_nearest_texture = nil;
    int width                                       = 0;
    int height                                      = 0;
    int input_channels                              = 0;
    bool preview_dirty                              = true;
    bool preview_params_valid                       = false;
    RendererPreviewControls last_preview_controls   = {};
    std::vector<float> source_rgba_pixels;
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
    __strong id<MTLLibrary> preview_library      = nil;
    __strong id<MTLRenderPipelineState> preview_pipeline = nil;
    __strong id<MTLSamplerState> linear_sampler  = nil;
    __strong id<MTLSamplerState> nearest_sampler = nil;
    bool imgui_initialized                       = false;
};

struct MetalPreviewUniforms {
    float exposure       = 0.0f;
    float gamma          = 1.0f;
    float offset         = 0.0f;
    int color_mode       = 0;
    int channel          = 0;
    int input_channels   = 0;
    int orientation      = 1;
    int _padding         = 0;
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

NSUInteger
align_up(NSUInteger value, NSUInteger alignment)
{
    if (alignment == 0)
        return value;
    const NSUInteger remainder = value % alignment;
    if (remainder == 0)
        return value;
    return value + (alignment - remainder);
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

bool
create_source_texture(id<MTLDevice> device, int width, int height,
                      const std::vector<float>& rgba_pixels,
                      id<MTLTexture>& texture, std::string& error_message)
{
    texture = nil;
    if (device == nil || width <= 0 || height <= 0) {
        error_message = "invalid Metal source texture parameters";
        return false;
    }
    const size_t expected = static_cast<size_t>(width) * height * 4;
    if (rgba_pixels.size() != expected) {
        error_message = "invalid Metal source pixel buffer size";
        return false;
    }
    MTLTextureDescriptor* descriptor
        = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
    descriptor.usage       = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    texture          = [device newTextureWithDescriptor:descriptor];
    if (texture == nil) {
        error_message = "failed to create Metal source texture";
        return false;
    }
    const MTLRegion region
        = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(width),
                          static_cast<NSUInteger>(height));
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:rgba_pixels.data()
               bytesPerRow:static_cast<NSUInteger>(width * 4
                                                   * sizeof(float))];
    error_message.clear();
    return true;
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
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    texture          = [device newTextureWithDescriptor:descriptor];
    if (texture == nil) {
        error_message = "failed to create Metal preview texture";
        return false;
    }
    error_message.clear();
    return true;
}

NSString*
preview_shader_source()
{
    static const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

struct PreviewUniforms {
    float exposure;
    float gamma;
    float offset;
    int color_mode;
    int channel;
    int input_channels;
    int orientation;
    int _padding;
};

vertex VertexOut imivPreviewVertex(uint vertex_id [[vertex_id]])
{
    const float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    const float2 uvs[3] = { float2(0.0, 0.0), float2(2.0, 0.0), float2(0.0, 2.0) };
    VertexOut out;
    out.position = float4(positions[vertex_id], 0.0, 1.0);
    out.uv = uvs[vertex_id];
    return out;
}

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

fragment float4 imivPreviewFragment(VertexOut in [[stage_in]],
                                    texture2d<float> source_texture [[texture(0)]],
                                    sampler source_sampler [[sampler(0)]],
                                    constant PreviewUniforms& uniforms [[buffer(0)]])
{
    float2 src_uv = display_to_source_uv(in.uv, uniforms.orientation);
    float4 rgba = source_texture.sample(source_sampler, src_uv);
    rgba.r += uniforms.offset;
    rgba.g += uniforms.offset;
    rgba.b += uniforms.offset;

    if (uniforms.color_mode == 1) {
        rgba.a = 1.0;
    } else if (uniforms.color_mode == 2) {
        float value = selected_channel(rgba, uniforms.channel);
        rgba = float4(value, value, value, 1.0);
    } else if (uniforms.color_mode == 3) {
        float value = dot(rgba.rgb, float3(0.2126, 0.7152, 0.0722));
        rgba = float4(value, value, value, 1.0);
    } else if (uniforms.color_mode == 4) {
        float value = selected_channel(rgba, uniforms.channel);
        rgba = float4(heatmap(value), 1.0);
    }

    if (uniforms.channel > 0 && uniforms.color_mode != 2 && uniforms.color_mode != 4) {
        float value = selected_channel(rgba, uniforms.channel);
        rgba = float4(value, value, value, 1.0);
    }

    if (uniforms.input_channels == 1 && uniforms.color_mode <= 1) {
        rgba.g = rgba.r;
        rgba.b = rgba.r;
        rgba.a = 1.0;
    } else if (uniforms.input_channels == 2 && uniforms.color_mode == 0) {
        rgba.g = rgba.r;
        rgba.b = rgba.r;
    } else if (uniforms.input_channels == 2 && uniforms.color_mode == 1) {
        rgba.g = rgba.r;
        rgba.b = rgba.r;
        rgba.a = 1.0;
    }

    float exposure_scale = exp2(uniforms.exposure);
    float inv_gamma = 1.0 / max(uniforms.gamma, 0.01f);
    rgba.rgb = pow(max(rgba.rgb * exposure_scale, float3(0.0)), float3(inv_gamma));
    rgba.rgb = clamp(rgba.rgb, 0.0, 1.0);
    rgba.a = clamp(rgba.a, 0.0, 1.0);
    return rgba;
}
)metal";
    return [NSString stringWithUTF8String:source];
}

bool
create_preview_pipeline(RendererBackendState& state, std::string& error_message)
{
    if (state.device == nil) {
        error_message = "Metal device is not initialized";
        return false;
    }
    NSError* error = nil;
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    state.preview_library = [state.device
        newLibraryWithSource:preview_shader_source()
                     options:options
                       error:&error];
    if (state.preview_library == nil) {
        error_message = (error != nil && error.localizedDescription != nil)
                            ? std::string(
                                  error.localizedDescription.UTF8String)
                            : "failed to compile Metal preview shader";
        return false;
    }

    id<MTLFunction> vertex_function = [state.preview_library
        newFunctionWithName:@"imivPreviewVertex"];
    id<MTLFunction> fragment_function = [state.preview_library
        newFunctionWithName:@"imivPreviewFragment"];
    if (vertex_function == nil || fragment_function == nil) {
        error_message = "failed to create Metal preview shader functions";
        return false;
    }

    MTLRenderPipelineDescriptor* descriptor
        = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction                 = vertex_function;
    descriptor.fragmentFunction               = fragment_function;
    descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    state.preview_pipeline = [state.device
        newRenderPipelineStateWithDescriptor:descriptor
                                       error:&error];
    if (state.preview_pipeline == nil) {
        error_message = (error != nil && error.localizedDescription != nil)
                            ? std::string(
                                  error.localizedDescription.UTF8String)
                            : "failed to create Metal preview pipeline";
        return false;
    }

    MTLSamplerDescriptor* linear_descriptor = [[MTLSamplerDescriptor alloc] init];
    linear_descriptor.minFilter             = MTLSamplerMinMagFilterLinear;
    linear_descriptor.magFilter             = MTLSamplerMinMagFilterLinear;
    linear_descriptor.sAddressMode          = MTLSamplerAddressModeClampToEdge;
    linear_descriptor.tAddressMode          = MTLSamplerAddressModeClampToEdge;
    state.linear_sampler = [state.device
        newSamplerStateWithDescriptor:linear_descriptor];

    MTLSamplerDescriptor* nearest_descriptor = [[MTLSamplerDescriptor alloc] init];
    nearest_descriptor.minFilter             = MTLSamplerMinMagFilterNearest;
    nearest_descriptor.magFilter             = MTLSamplerMinMagFilterNearest;
    nearest_descriptor.sAddressMode          = MTLSamplerAddressModeClampToEdge;
    nearest_descriptor.tAddressMode          = MTLSamplerAddressModeClampToEdge;
    state.nearest_sampler = [state.device
        newSamplerStateWithDescriptor:nearest_descriptor];

    if (state.linear_sampler == nil || state.nearest_sampler == nil) {
        error_message = "failed to create Metal preview samplers";
        return false;
    }

    error_message.clear();
    return true;
}

bool
render_preview_texture(RendererBackendState& state,
                       RendererTextureBackendState& texture_state,
                       id<MTLTexture> target_texture,
                       id<MTLSamplerState> sampler_state,
                       const RendererPreviewControls& controls,
                       std::string& error_message)
{
    if (state.command_queue == nil || state.preview_pipeline == nil
        || sampler_state == nil || texture_state.source_texture == nil
        || target_texture == nil) {
        error_message = "Metal preview pipeline state is not initialized";
        return false;
    }

    MetalPreviewUniforms uniforms;
    uniforms.exposure       = controls.exposure;
    uniforms.gamma          = std::max(controls.gamma, 0.01f);
    uniforms.offset         = controls.offset;
    uniforms.color_mode     = controls.color_mode;
    uniforms.channel        = controls.channel;
    uniforms.input_channels = texture_state.input_channels;
    uniforms.orientation    = controls.orientation;

    id<MTLCommandBuffer> command_buffer = [state.command_queue commandBuffer];
    if (command_buffer == nil) {
        error_message = "failed to create Metal preview command buffer";
        return false;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture     = target_texture;
    pass.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> encoder = [command_buffer
        renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
        error_message = "failed to create Metal preview encoder";
        return false;
    }

    [encoder setRenderPipelineState:state.preview_pipeline];
    [encoder setFragmentTexture:texture_state.source_texture atIndex:0];
    [encoder setFragmentSamplerState:sampler_state atIndex:0];
    [encoder setFragmentBytes:&uniforms
                       length:sizeof(uniforms)
                      atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:3];
    [encoder endEncoding];
    [command_buffer commit];
    [command_buffer waitUntilCompleted];
    if (command_buffer.status != MTLCommandBufferStatusCompleted) {
        error_message = "Metal preview render failed";
        return false;
    }
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
        || !create_source_texture(state->device, image.width, image.height,
                                  texture_state->source_rgba_pixels,
                                  texture_state->source_texture,
                                  error_message)
        || !create_preview_texture(state->device, image.width, image.height,
                                   texture_state->preview_linear_texture,
                                   error_message)
        || !create_preview_texture(state->device, image.width, image.height,
                                   texture_state->preview_nearest_texture,
                                   error_message)) {
        texture_state->source_texture          = nil;
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
    state->source_texture          = nil;
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
    (void)image;
    (void)ui_state;

    RendererBackendState* renderer_backend = backend_state(renderer_state);
    RendererTextureBackendState* texture_state = texture_backend_state(texture);
    if (renderer_backend == nullptr || texture_state == nullptr) {
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

    if (!render_preview_texture(*renderer_backend, *texture_state,
                                texture_state->preview_linear_texture,
                                renderer_backend->linear_sampler,
                                effective_controls, error_message)
        || !render_preview_texture(*renderer_backend, *texture_state,
                                   texture_state->preview_nearest_texture,
                                   renderer_backend->nearest_sampler,
                                   effective_controls, error_message)) {
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
    if (!create_preview_pipeline(*state, error_message))
        return false;
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
    state->layer.framebufferOnly  = NO;
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
        || state->current_drawable == nil) {
        return;
    }
    if (state->current_encoder != nil)
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
    RendererState* renderer_state = reinterpret_cast<RendererState*>(user_data);
    if (renderer_state == nullptr || pixels == nullptr || w <= 0 || h <= 0)
        return false;

    RendererBackendState* state = backend_state(*renderer_state);
    if (state == nullptr || state->current_drawable == nil
        || state->current_command_buffer == nil
        || state->current_encoder == nil) {
        return false;
    }

    id<MTLTexture> texture = state->current_drawable.texture;
    if (texture == nil)
        return false;

    const int texture_width  = static_cast<int>(texture.width);
    const int texture_height = static_cast<int>(texture.height);
    int window_width         = 0;
    int window_height        = 0;
    if (state->window != nullptr)
        glfwGetWindowSize(state->window, &window_width, &window_height);

    const double scale_x = (window_width > 0)
                               ? static_cast<double>(texture_width)
                                     / static_cast<double>(window_width)
                               : 1.0;
    const double scale_y = (window_height > 0)
                               ? static_cast<double>(texture_height)
                                     / static_cast<double>(window_height)
                               : 1.0;
    const bool logical_rect = (window_width > 0 && window_height > 0 && w > 0
                               && h > 0 && w <= window_width
                               && h <= window_height
                               && (std::abs(scale_x - 1.0) > 1.0e-3
                                   || std::abs(scale_y - 1.0) > 1.0e-3));

    const int src_x = logical_rect
                          ? static_cast<int>(std::lround(x * scale_x))
                          : x;
    const int src_y = logical_rect
                          ? static_cast<int>(std::lround(y * scale_y))
                          : y;
    const int src_w = logical_rect
                          ? std::max(1, static_cast<int>(std::lround(
                                           static_cast<double>(w) * scale_x)))
                          : w;
    const int src_h = logical_rect
                          ? std::max(1, static_cast<int>(std::lround(
                                           static_cast<double>(h) * scale_y)))
                          : h;
    if (src_x < 0 || src_y < 0 || src_x + src_w > texture_width
        || src_y + src_h > texture_height) {
        return false;
    }

    [state->current_encoder endEncoding];
    state->current_encoder = nil;

    const NSUInteger bytes_per_pixel = 4;
    const NSUInteger row_bytes = align_up(
        static_cast<NSUInteger>(src_w) * bytes_per_pixel, 256);
    const NSUInteger buffer_size = row_bytes * static_cast<NSUInteger>(src_h);

    id<MTLBuffer> readback_buffer = [state->device
        newBufferWithLength:buffer_size
                    options:MTLResourceStorageModeShared];
    if (readback_buffer == nil)
        return false;

    id<MTLBlitCommandEncoder> blit = [state->current_command_buffer
        blitCommandEncoder];
    if (blit == nil)
        return false;

    const MTLOrigin origin = MTLOriginMake(src_x, src_y, 0);
    const MTLSize size     = MTLSizeMake(src_w, src_h, 1);
    [blit copyFromTexture:texture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:origin
               sourceSize:size
                 toBuffer:readback_buffer
        destinationOffset:0
   destinationBytesPerRow:row_bytes
 destinationBytesPerImage:buffer_size];
    [blit endEncoding];

    [state->current_command_buffer presentDrawable:state->current_drawable];
    [state->current_command_buffer commit];
    [state->current_command_buffer waitUntilCompleted];

    if (state->current_command_buffer.status != MTLCommandBufferStatusCompleted)
        return false;

    const unsigned char* src_bytes = static_cast<const unsigned char*>(
        [readback_buffer contents]);
    if (src_bytes == nullptr)
        return false;

    unsigned char* dst_bytes = reinterpret_cast<unsigned char*>(pixels);
    const double sample_scale_x = static_cast<double>(src_w)
                                  / static_cast<double>(w);
    const double sample_scale_y = static_cast<double>(src_h)
                                  / static_cast<double>(h);
    for (int row = 0; row < h; ++row) {
        unsigned char* dst_row = dst_bytes
                                 + static_cast<size_t>(row)
                                       * static_cast<size_t>(w) * 4;
        const int sample_row = std::clamp(
            static_cast<int>(std::floor((static_cast<double>(row) + 0.5)
                                        * sample_scale_y)),
            0, src_h - 1);
        const unsigned char* src_row
            = src_bytes + static_cast<size_t>(sample_row)
                              * static_cast<size_t>(row_bytes);
        for (int col = 0; col < w; ++col) {
            const int sample_col = std::clamp(
                static_cast<int>(std::floor((static_cast<double>(col) + 0.5)
                                            * sample_scale_x)),
                0, src_w - 1);
            const unsigned char* src
                = src_row + static_cast<size_t>(sample_col) * 4;
            unsigned char* dst       = dst_row + static_cast<size_t>(col) * 4;
            dst[0]                   = src[2];
            dst[1]                   = src[1];
            dst[2]                   = src[0];
            dst[3]                   = src[3];
        }
    }

    state->current_command_buffer = nil;
    state->current_drawable       = nil;
    return true;
}

}  // namespace Imiv
