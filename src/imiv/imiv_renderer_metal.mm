// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_loaded_image.h"
#include "imiv_ocio.h"
#include "imiv_platform_glfw.h"
#include "imiv_preview_shader_text.h"
#include "imiv_tiling.h"
#include "imiv_viewer.h"

#include "imiv_imgui_metal_extras.h"

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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Imiv {

namespace {

    struct RendererTextureBackendState {
        __strong id<MTLTexture> source_texture          = nil;
        __strong id<MTLTexture> preview_linear_texture  = nil;
        __strong id<MTLTexture> preview_nearest_texture = nil;
        ImTextureID preview_linear_tex_id               = ImTextureID_Invalid;
        ImTextureID preview_nearest_tex_id              = ImTextureID_Invalid;
        int width                                       = 0;
        int height                                      = 0;
        int input_channels                              = 0;
        bool preview_dirty                              = true;
        bool preview_params_valid                       = false;
        PreviewControls last_preview_controls           = {};
    };

    struct MetalOcioTextureBinding {
        std::string texture_name;
        std::string sampler_name;
        __strong id<MTLTexture> texture      = nil;
        __strong id<MTLSamplerState> sampler = nil;
        NSUInteger texture_index             = 0;
        NSUInteger sampler_index             = 0;
    };

    struct MetalOcioVectorUniformBinding {
        std::string name;
        OcioUniformType type = OcioUniformType::Unknown;
        std::vector<unsigned char> bytes;
        NSUInteger buffer_index = 0;
    };

    struct MetalOcioPreviewState {
        OcioShaderRuntime* runtime                   = nullptr;
        __strong id<MTLLibrary> library              = nil;
        __strong id<MTLRenderPipelineState> pipeline = nil;
        std::vector<MetalOcioTextureBinding> textures;
        std::vector<MetalOcioVectorUniformBinding> vector_uniforms;
        std::string shader_cache_id;
        bool ready = false;
    };

    struct RendererBackendState {
        GLFWwindow* window                                   = nullptr;
        id<MTLDevice> device                                 = nil;
        id<MTLCommandQueue> command_queue                    = nil;
        CAMetalLayer* layer                                  = nil;
        MTLRenderPassDescriptor* render_pass                 = nil;
        id<CAMetalDrawable> current_drawable                 = nil;
        id<MTLCommandBuffer> current_command_buffer          = nil;
        id<MTLRenderCommandEncoder> current_encoder          = nil;
        __strong id<MTLLibrary> preview_library              = nil;
        __strong id<MTLRenderPipelineState> preview_pipeline = nil;
        __strong id<MTLComputePipelineState> upload_pipeline = nil;
        __strong id<MTLSamplerState> linear_sampler          = nil;
        __strong id<MTLSamplerState> nearest_sampler         = nil;
        MetalOcioPreviewState ocio_preview;
        bool imgui_initialized = false;
    };

    struct MetalPreviewUniforms {
        float exposure     = 0.0f;
        float gamma        = 1.0f;
        float offset       = 0.0f;
        int color_mode     = 0;
        int channel        = 0;
        int input_channels = 0;
        int orientation    = 1;
        int _padding       = 0;
    };

    struct MetalUploadUniforms {
        uint32_t width              = 0;
        uint32_t height             = 0;
        uint32_t dst_y_offset       = 0;
        uint32_t row_pitch_bytes    = 0;
        uint32_t pixel_stride_bytes = 0;
        uint32_t channel_count      = 0;
        uint32_t data_type          = 0;
    };

    constexpr size_t kDefaultMetalUploadChunkBytes = 64u * 1024u * 1024u;

    RendererBackendState* backend_state(RendererState& renderer_state)
    {
        return reinterpret_cast<RendererBackendState*>(renderer_state.backend);
    }

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
        renderer_state.backend = reinterpret_cast<::Imiv::RendererBackendState*>(
            new RendererBackendState());
        return renderer_state.backend != nullptr;
    }

    NSUInteger align_up(NSUInteger value, NSUInteger alignment)
    {
        if (alignment == 0)
            return value;
        const NSUInteger remainder = value % alignment;
        if (remainder == 0)
            return value;
        return value + (alignment - remainder);
    }

    bool read_metal_limit_override(const char* name, size_t& out_value)
    {
        out_value         = 0;
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0')
            return false;
        char* end              = nullptr;
        unsigned long long raw = std::strtoull(value, &end, 10);
        if (end == value || *end != '\0' || raw == 0
            || raw > static_cast<unsigned long long>(
                   std::numeric_limits<size_t>::max())) {
            return false;
        }
        out_value = static_cast<size_t>(raw);
        return true;
    }

    size_t metal_max_upload_chunk_bytes()
    {
        size_t override_value = 0;
        if (read_metal_limit_override(
                "IMIV_METAL_MAX_UPLOAD_CHUNK_BYTES_OVERRIDE", override_value)) {
            return override_value;
        }
        return kDefaultMetalUploadChunkBytes;
    }

    void update_drawable_size(RendererState& renderer_state)
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

    bool preview_controls_equal(const PreviewControls& a,
                                const PreviewControls& b)
    {
        return std::abs(a.exposure - b.exposure) < 1.0e-6f
               && std::abs(a.gamma - b.gamma) < 1.0e-6f
               && std::abs(a.offset - b.offset) < 1.0e-6f
               && a.color_mode == b.color_mode && a.channel == b.channel
               && a.use_ocio == b.use_ocio && a.orientation == b.orientation
               && a.linear_interpolation == b.linear_interpolation;
    }

    bool prepare_source_upload(const LoadedImage& image,
                               const unsigned char*& upload_ptr,
                               size_t& upload_bytes,
                               UploadDataType& upload_type,
                               size_t& channel_bytes, size_t& row_pitch_bytes,
                               std::vector<unsigned char>& converted_pixels,
                               std::string& error_message)
    {
        error_message.clear();
        converted_pixels.clear();
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0
            || image.pixels.empty()) {
            error_message = "invalid source image dimensions";
            return false;
        }

        upload_type     = image.type;
        channel_bytes   = image.channel_bytes;
        row_pitch_bytes = image.row_pitch_bytes;
        upload_ptr      = image.pixels.data();
        upload_bytes    = image.pixels.size();

        if (upload_type == UploadDataType::Unknown) {
            error_message = "unsupported source pixel type";
            return false;
        }

        const size_t channel_count = static_cast<size_t>(image.nchannels);
        if (upload_type == UploadDataType::Double) {
            const size_t value_count = image.pixels.size() / sizeof(double);
            converted_pixels.resize(value_count * sizeof(float));
            const double* src = reinterpret_cast<const double*>(
                image.pixels.data());
            float* dst = reinterpret_cast<float*>(converted_pixels.data());
            for (size_t i = 0; i < value_count; ++i)
                dst[i] = static_cast<float>(src[i]);
            upload_type     = UploadDataType::Float;
            channel_bytes   = sizeof(float);
            row_pitch_bytes = static_cast<size_t>(image.width) * channel_count
                              * channel_bytes;
            upload_ptr   = converted_pixels.data();
            upload_bytes = converted_pixels.size();
        }

        const size_t pixel_stride_bytes = channel_bytes * channel_count;
        if (converted_pixels.empty()) {
            LoadedImageLayout layout;
            if (!describe_loaded_image_layout(image, layout, error_message)) {
                if (error_message == "invalid source row pitch")
                    error_message = "invalid source row pitch";
                return false;
            }
            row_pitch_bytes = image.row_pitch_bytes;
            upload_bytes    = layout.required_bytes;
        } else {
            if (pixel_stride_bytes == 0 || row_pitch_bytes == 0
                || row_pitch_bytes < static_cast<size_t>(image.width)
                                         * pixel_stride_bytes) {
                error_message = "invalid source row pitch";
                return false;
            }
            const size_t required_bytes = row_pitch_bytes
                                          * static_cast<size_t>(image.height);
            if (upload_bytes < required_bytes) {
                error_message
                    = "source pixel buffer is smaller than declared stride";
                return false;
            }
            upload_bytes = required_bytes;
        }

        if (row_pitch_bytes > std::numeric_limits<uint32_t>::max()
            || pixel_stride_bytes > std::numeric_limits<uint32_t>::max()) {
            error_message = "source image stride exceeds Metal upload limits";
            return false;
        }

        return true;
    }

    void rgb_to_rgba(const float* rgb_values, size_t value_count,
                     std::vector<float>& rgba_values)
    {
        rgba_values.clear();
        if (rgb_values == nullptr || value_count == 0)
            return;
        rgba_values.reserve((value_count / 3u) * 4u);
        for (size_t i = 0; i + 2 < value_count; i += 3) {
            rgba_values.push_back(rgb_values[i + 0]);
            rgba_values.push_back(rgb_values[i + 1]);
            rgba_values.push_back(rgb_values[i + 2]);
            rgba_values.push_back(1.0f);
        }
    }

    id<MTLSamplerState> create_sampler_state(id<MTLDevice> device,
                                             OcioInterpolation interpolation)
    {
        if (device == nil)
            return nil;
        MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
        if (interpolation == OcioInterpolation::Nearest) {
            descriptor.minFilter = MTLSamplerMinMagFilterNearest;
            descriptor.magFilter = MTLSamplerMinMagFilterNearest;
        } else {
            descriptor.minFilter = MTLSamplerMinMagFilterLinear;
            descriptor.magFilter = MTLSamplerMinMagFilterLinear;
        }
        descriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        descriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
        return [device newSamplerStateWithDescriptor:descriptor];
    }

    std::string metal_error_string(NSError* error, const char* fallback_message)
    {
        if (error != nil && error.localizedDescription != nil)
            return std::string(error.localizedDescription.UTF8String);
        return std::string(fallback_message);
    }

    bool create_shader_library(id<MTLDevice> device, NSString* source,
                               MTLCompileOptions* options,
                               const char* device_error,
                               const char* compile_error,
                               id<MTLLibrary>& library,
                               std::string& error_message)
    {
        library = nil;
        if (device == nil) {
            error_message = device_error;
            return false;
        }

        NSError* error = nil;
        library        = [device newLibraryWithSource:source
                                              options:options
                                                error:&error];
        if (library == nil) {
            error_message = metal_error_string(error, compile_error);
            return false;
        }

        error_message.clear();
        return true;
    }

    bool create_compute_pipeline_state(
        id<MTLDevice> device, id<MTLLibrary> library, const char* function_name,
        const char* function_error, const char* pipeline_error,
        id<MTLComputePipelineState>& pipeline, std::string& error_message)
    {
        pipeline = nil;
        if (device == nil) {
            error_message = "Metal device is not initialized";
            return false;
        }
        if (library == nil) {
            error_message = function_error;
            return false;
        }

        NSString* ns_function_name
            = [NSString stringWithUTF8String:function_name];
        id<MTLFunction> function = [library
            newFunctionWithName:ns_function_name];
        if (function == nil) {
            error_message = function_error;
            return false;
        }

        NSError* error = nil;
        pipeline       = [device newComputePipelineStateWithFunction:function
                                                               error:&error];
        if (pipeline == nil) {
            error_message = metal_error_string(error, pipeline_error);
            return false;
        }

        error_message.clear();
        return true;
    }

    bool create_render_pipeline_state(
        id<MTLDevice> device, id<MTLLibrary> library, const char* vertex_name,
        const char* fragment_name, const char* function_error,
        const char* pipeline_error, id<MTLRenderPipelineState>& pipeline,
        std::string& error_message)
    {
        pipeline = nil;
        if (device == nil) {
            error_message = "Metal device is not initialized";
            return false;
        }
        if (library == nil) {
            error_message = function_error;
            return false;
        }

        NSString* ns_vertex_name   = [NSString stringWithUTF8String:vertex_name];
        NSString* ns_fragment_name = [NSString
            stringWithUTF8String:fragment_name];
        id<MTLFunction> vertex_function = [library
            newFunctionWithName:ns_vertex_name];
        id<MTLFunction> fragment_function = [library
            newFunctionWithName:ns_fragment_name];
        if (vertex_function == nil || fragment_function == nil) {
            error_message = function_error;
            return false;
        }

        MTLRenderPipelineDescriptor* descriptor =
            [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.vertexFunction                  = vertex_function;
        descriptor.fragmentFunction                = fragment_function;
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        NSError* error = nil;
        pipeline       = [device newRenderPipelineStateWithDescriptor:descriptor
                                                                error:&error];
        if (pipeline == nil) {
            error_message = metal_error_string(error, pipeline_error);
            return false;
        }

        error_message.clear();
        return true;
    }

    bool create_ocio_texture(id<MTLDevice> device,
                             const OcioTextureBlueprint& blueprint,
                             id<MTLTexture>& texture,
                             std::string& error_message)
    {
        texture = nil;
        if (device == nil) {
            error_message = "Metal OCIO device is not initialized";
            return false;
        }
        if (blueprint.values.empty()) {
            error_message = "missing Metal OCIO LUT values";
            return false;
        }

        std::vector<float> adapted_values;
        const float* values              = nullptr;
        MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
        descriptor.mipmapLevelCount      = 1;

        if (blueprint.dimensions == OcioTextureDimensions::Tex3D) {
            if (blueprint.width == 0 || blueprint.height == 0
                || blueprint.depth == 0) {
                error_message = "invalid Metal OCIO 3D LUT dimensions";
                return false;
            }
            rgb_to_rgba(blueprint.values.data(), blueprint.values.size(),
                        adapted_values);
            values                 = adapted_values.data();
            descriptor.textureType = MTLTextureType3D;
            descriptor.pixelFormat = MTLPixelFormatRGBA32Float;
            descriptor.width       = blueprint.width;
            descriptor.height      = blueprint.height;
            descriptor.depth       = blueprint.depth;
            texture = [device newTextureWithDescriptor:descriptor];
            if (texture == nil) {
                error_message = "failed to create Metal OCIO 3D LUT texture";
                return false;
            }
            [texture
                replaceRegion:MTLRegionMake3D(0, 0, 0, blueprint.width,
                                              blueprint.height, blueprint.depth)
                  mipmapLevel:0
                        slice:0
                    withBytes:values
                  bytesPerRow:static_cast<NSUInteger>(blueprint.width) * 4u
                              * sizeof(float)
                bytesPerImage:static_cast<NSUInteger>(blueprint.width)
                              * static_cast<NSUInteger>(blueprint.height) * 4u
                              * sizeof(float)];
            error_message.clear();
            return true;
        }

        if (blueprint.width == 0 || blueprint.height == 0) {
            error_message = "invalid Metal OCIO LUT dimensions";
            return false;
        }

        const bool red_channel = blueprint.channel == OcioTextureChannel::Red;
        if (red_channel) {
            adapted_values = blueprint.values;
        } else {
            rgb_to_rgba(blueprint.values.data(), blueprint.values.size(),
                        adapted_values);
        }
        values = adapted_values.data();

        descriptor.textureType = blueprint.dimensions
                                         == OcioTextureDimensions::Tex1D
                                     ? MTLTextureType1D
                                     : MTLTextureType2D;
        descriptor.pixelFormat = red_channel ? MTLPixelFormatR32Float
                                             : MTLPixelFormatRGBA32Float;
        descriptor.width       = blueprint.width;
        descriptor.height = blueprint.dimensions == OcioTextureDimensions::Tex1D
                                ? 1u
                                : blueprint.height;
        descriptor.depth  = 1;
        texture           = [device newTextureWithDescriptor:descriptor];
        if (texture == nil) {
            error_message = "failed to create Metal OCIO LUT texture";
            return false;
        }
        [texture replaceRegion:MTLRegionMake3D(0, 0, 0, descriptor.width,
                                               descriptor.height, 1)
                   mipmapLevel:0
                     withBytes:values
                   bytesPerRow:static_cast<NSUInteger>(descriptor.width)
                               * static_cast<NSUInteger>(red_channel ? 1u : 4u)
                               * sizeof(float)];
        error_message.clear();
        return true;
    }

    void destroy_ocio_preview_resources(MetalOcioPreviewState& state)
    {
        for (MetalOcioTextureBinding& texture : state.textures) {
            texture.texture = nil;
            texture.sampler = nil;
        }
        state.textures.clear();
        state.vector_uniforms.clear();
        state.library  = nil;
        state.pipeline = nil;
        state.shader_cache_id.clear();
        state.ready = false;
    }

    void destroy_ocio_preview_program(MetalOcioPreviewState& state)
    {
        destroy_ocio_preview_resources(state);
        destroy_ocio_shader_runtime(state.runtime);
    }

    void align_uniform_bytes(std::vector<unsigned char>& bytes,
                             size_t alignment)
    {
        if (alignment <= 1)
            return;
        const size_t remainder = bytes.size() % alignment;
        if (remainder == 0)
            return;
        bytes.resize(bytes.size() + (alignment - remainder), 0);
    }

    std::string uniform_count_name(const std::string& name)
    {
        return name + "_count";
    }

    OcioUniformType metal_ocio_uniform_type(OCIO::UniformDataType type)
    {
        switch (type) {
        case OCIO::UNIFORM_DOUBLE: return OcioUniformType::Double;
        case OCIO::UNIFORM_BOOL: return OcioUniformType::Bool;
        case OCIO::UNIFORM_FLOAT3: return OcioUniformType::Float3;
        case OCIO::UNIFORM_VECTOR_FLOAT: return OcioUniformType::VectorFloat;
        case OCIO::UNIFORM_VECTOR_INT: return OcioUniformType::VectorInt;
        case OCIO::UNIFORM_UNKNOWN:
        default: return OcioUniformType::Unknown;
        }
    }

    bool build_ocio_scalar_uniform_bytes(OcioShaderRuntime& runtime,
                                         std::vector<unsigned char>& bytes,
                                         std::string& error_message)
    {
        bytes.clear();
        error_message.clear();
        if (runtime.shader_desc == nullptr)
            return true;

        const unsigned num_uniforms = runtime.shader_desc->getNumUniforms();
        size_t max_alignment        = 4;
        for (unsigned idx = 0; idx < num_uniforms; ++idx) {
            OCIO::GpuShaderDesc::UniformData data;
            runtime.shader_desc->getUniform(idx, data);
            switch (data.m_type) {
            case OCIO::UNIFORM_DOUBLE: {
                align_uniform_bytes(bytes, 4);
                const float value = data.m_getDouble
                                        ? static_cast<float>(data.m_getDouble())
                                        : 0.0f;
                const unsigned char* src
                    = reinterpret_cast<const unsigned char*>(&value);
                bytes.insert(bytes.end(), src, src + sizeof(value));
                break;
            }
            case OCIO::UNIFORM_BOOL: {
                align_uniform_bytes(bytes, 4);
                const int32_t value = (data.m_getBool && data.m_getBool()) ? 1
                                                                           : 0;
                const unsigned char* src
                    = reinterpret_cast<const unsigned char*>(&value);
                bytes.insert(bytes.end(), src, src + sizeof(value));
                break;
            }
            case OCIO::UNIFORM_FLOAT3: {
                align_uniform_bytes(bytes, 16);
                max_alignment   = std::max(max_alignment, size_t(16));
                float packed[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (data.m_getFloat3) {
                    const OCIO::Float3& value = data.m_getFloat3();
                    packed[0]                 = value[0];
                    packed[1]                 = value[1];
                    packed[2]                 = value[2];
                }
                const unsigned char* src
                    = reinterpret_cast<const unsigned char*>(packed);
                bytes.insert(bytes.end(), src, src + sizeof(packed));
                break;
            }
            case OCIO::UNIFORM_VECTOR_FLOAT: {
                align_uniform_bytes(bytes, 4);
                const int32_t count = data.m_vectorFloat.m_getSize
                                          ? data.m_vectorFloat.m_getSize()
                                          : 0;
                const unsigned char* src
                    = reinterpret_cast<const unsigned char*>(&count);
                bytes.insert(bytes.end(), src, src + sizeof(count));
                break;
            }
            case OCIO::UNIFORM_VECTOR_INT: {
                align_uniform_bytes(bytes, 4);
                const int32_t count = data.m_vectorInt.m_getSize
                                          ? data.m_vectorInt.m_getSize()
                                          : 0;
                const unsigned char* src
                    = reinterpret_cast<const unsigned char*>(&count);
                bytes.insert(bytes.end(), src, src + sizeof(count));
                break;
            }
            case OCIO::UNIFORM_UNKNOWN:
            default:
                error_message = "unsupported Metal OCIO uniform type";
                return false;
            }
        }

        align_uniform_bytes(bytes, max_alignment);
        return true;
    }

    bool build_ocio_vector_uniform_bindings(
        OcioShaderRuntime& runtime,
        std::vector<MetalOcioVectorUniformBinding>& bindings,
        std::string& error_message)
    {
        bindings.clear();
        error_message.clear();
        if (runtime.shader_desc == nullptr)
            return true;

        NSUInteger buffer_index     = 2;
        const unsigned num_uniforms = runtime.shader_desc->getNumUniforms();
        for (unsigned idx = 0; idx < num_uniforms; ++idx) {
            OCIO::GpuShaderDesc::UniformData data;
            const char* name = runtime.shader_desc->getUniform(idx, data);
            if (data.m_type != OCIO::UNIFORM_VECTOR_FLOAT
                && data.m_type != OCIO::UNIFORM_VECTOR_INT) {
                continue;
            }

            MetalOcioVectorUniformBinding binding;
            if (name != nullptr)
                binding.name = name;
            binding.type         = metal_ocio_uniform_type(data.m_type);
            binding.buffer_index = buffer_index++;

            if (data.m_type == OCIO::UNIFORM_VECTOR_FLOAT) {
                const int count = data.m_vectorFloat.m_getSize
                                      ? data.m_vectorFloat.m_getSize()
                                      : 0;
                if (count > 0 && data.m_vectorFloat.m_getVector) {
                    const float* src = data.m_vectorFloat.m_getVector();
                    const unsigned char* begin
                        = reinterpret_cast<const unsigned char*>(src);
                    binding.bytes.assign(begin, begin
                                                    + static_cast<size_t>(count)
                                                          * sizeof(float));
                } else {
                    const float dummy = 0.0f;
                    const unsigned char* begin
                        = reinterpret_cast<const unsigned char*>(&dummy);
                    binding.bytes.assign(begin, begin + sizeof(dummy));
                }
            } else {
                const int count = data.m_vectorInt.m_getSize
                                      ? data.m_vectorInt.m_getSize()
                                      : 0;
                if (count > 0 && data.m_vectorInt.m_getVector) {
                    const int* src = data.m_vectorInt.m_getVector();
                    const unsigned char* begin
                        = reinterpret_cast<const unsigned char*>(src);
                    binding.bytes.assign(begin, begin
                                                    + static_cast<size_t>(count)
                                                          * sizeof(int));
                } else {
                    const int dummy = 0;
                    const unsigned char* begin
                        = reinterpret_cast<const unsigned char*>(&dummy);
                    binding.bytes.assign(begin, begin + sizeof(dummy));
                }
            }

            bindings.push_back(std::move(binding));
        }

        return true;
    }

    bool create_source_texture(id<MTLDevice> device, int width, int height,
                               id<MTLTexture>& texture,
                               std::string& error_message)
    {
        texture = nil;
        if (device == nil || width <= 0 || height <= 0) {
            error_message = "invalid Metal source texture parameters";
            return false;
        }
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
        descriptor.usage                 = MTLTextureUsageShaderRead
                           | MTLTextureUsageShaderWrite;
        descriptor.storageMode = MTLStorageModePrivate;
        texture                = [device newTextureWithDescriptor:descriptor];
        if (texture == nil) {
            error_message = "failed to create Metal source texture";
            return false;
        }
        error_message.clear();
        return true;
    }

    NSString* upload_shader_source()
    {
        static const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;

struct UploadUniforms {
    uint width;
    uint height;
    uint dst_y_offset;
    uint row_pitch_bytes;
    uint pixel_stride_bytes;
    uint channel_count;
    uint data_type;
};

constant uint IMIV_DATA_U8  = 0u;
constant uint IMIV_DATA_U16 = 1u;
constant uint IMIV_DATA_U32 = 2u;
constant uint IMIV_DATA_F16 = 3u;
constant uint IMIV_DATA_F32 = 4u;
constant uint IMIV_DATA_F64 = 5u;

inline uint read_byte(const device uchar* src_bytes, uint byte_offset)
{
    return uint(src_bytes[byte_offset]);
}

inline uint read_u16(const device uchar* src_bytes, uint byte_offset)
{
    return read_byte(src_bytes, byte_offset)
           | (read_byte(src_bytes, byte_offset + 1u) << 8u);
}

inline uint read_u32(const device uchar* src_bytes, uint byte_offset)
{
    return read_byte(src_bytes, byte_offset)
           | (read_byte(src_bytes, byte_offset + 1u) << 8u)
           | (read_byte(src_bytes, byte_offset + 2u) << 16u)
           | (read_byte(src_bytes, byte_offset + 3u) << 24u);
}

inline float decode_channel(const device uchar* src_bytes,
                            constant UploadUniforms& upload,
                            uint pixel_offset, uint channel_index)
{
    uint channel_bytes = 1u;
    if (upload.data_type == IMIV_DATA_U16 || upload.data_type == IMIV_DATA_F16)
        channel_bytes = 2u;
    else if (upload.data_type == IMIV_DATA_U32
             || upload.data_type == IMIV_DATA_F32)
        channel_bytes = 4u;
    else if (upload.data_type == IMIV_DATA_F64)
        channel_bytes = 8u;

    const uint byte_offset = pixel_offset + channel_index * channel_bytes;
    if (upload.data_type == IMIV_DATA_U8)
        return float(read_byte(src_bytes, byte_offset)) * (1.0f / 255.0f);
    if (upload.data_type == IMIV_DATA_U16)
        return float(read_u16(src_bytes, byte_offset)) * (1.0f / 65535.0f);
    if (upload.data_type == IMIV_DATA_U32)
        return float(read_u32(src_bytes, byte_offset))
               * (1.0f / 4294967295.0f);
    if (upload.data_type == IMIV_DATA_F16)
        return float(as_type<half>(ushort(read_u16(src_bytes, byte_offset))));
    if (upload.data_type == IMIV_DATA_F32)
        return as_type<float>(read_u32(src_bytes, byte_offset));
    return 0.0f;
}

inline float4 decode_pixel(const device uchar* src_bytes,
                           constant UploadUniforms& upload, uint pixel_offset)
{
    if (upload.channel_count == 0u)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (upload.channel_count == 1u) {
        const float g = decode_channel(src_bytes, upload, pixel_offset, 0u);
        return float4(g, g, g, 1.0f);
    }
    if (upload.channel_count == 2u) {
        const float g = decode_channel(src_bytes, upload, pixel_offset, 0u);
        const float a = decode_channel(src_bytes, upload, pixel_offset, 1u);
        return float4(g, g, g, a);
    }
    const float r = decode_channel(src_bytes, upload, pixel_offset, 0u);
    const float g = decode_channel(src_bytes, upload, pixel_offset, 1u);
    const float b = decode_channel(src_bytes, upload, pixel_offset, 2u);
    float a = 1.0f;
    if (upload.channel_count >= 4u)
        a = decode_channel(src_bytes, upload, pixel_offset, 3u);
    return float4(r, g, b, a);
}

kernel void imivUploadToSourceTexture(const device uchar* src_bytes [[buffer(0)]],
                                      constant UploadUniforms& upload [[buffer(1)]],
                                      texture2d<float, access::write> dst_texture [[texture(0)]],
                                      uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= upload.width || gid.y >= upload.height)
        return;
    const uint pixel_offset = gid.y * upload.row_pitch_bytes
                              + gid.x * upload.pixel_stride_bytes;
    dst_texture.write(decode_pixel(src_bytes, upload, pixel_offset),
                      uint2(gid.x, gid.y + upload.dst_y_offset));
}
)metal";
        return [NSString stringWithUTF8String:source];
    }

    bool create_upload_pipeline(RendererBackendState& state,
                                std::string& error_message)
    {
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        id<MTLLibrary> library     = nil;
        if (!create_shader_library(state.device, upload_shader_source(),
                                   options, "Metal device is not initialized",
                                   "failed to compile Metal upload shader",
                                   library, error_message)) {
            return false;
        }
        return create_compute_pipeline_state(
            state.device, library, "imivUploadToSourceTexture",
            "failed to create Metal upload shader function",
            "failed to create Metal upload pipeline", state.upload_pipeline,
            error_message);
    }

    bool upload_source_texture(RendererBackendState& state,
                               const LoadedImage& image, id<MTLTexture> texture,
                               std::string& error_message)
    {
        if (state.device == nil || state.command_queue == nil || texture == nil
            || state.upload_pipeline == nil) {
            error_message = "Metal upload pipeline is not initialized";
            return false;
        }

        std::vector<unsigned char> converted_pixels;
        const unsigned char* upload_ptr = nullptr;
        size_t upload_bytes             = 0;
        UploadDataType upload_type      = UploadDataType::Unknown;
        size_t channel_bytes            = 0;
        size_t row_pitch_bytes          = 0;
        if (!prepare_source_upload(image, upload_ptr, upload_bytes, upload_type,
                                   channel_bytes, row_pitch_bytes,
                                   converted_pixels, error_message)) {
            return false;
        }

        const size_t pixel_stride_bytes
            = channel_bytes * static_cast<size_t>(image.nchannels);
        RowStripeUploadPlan stripe_plan;
        if (!build_row_stripe_upload_plan(row_pitch_bytes, pixel_stride_bytes,
                                          image.height,
                                          metal_max_upload_chunk_bytes(), 1,
                                          stripe_plan, error_message)) {
            return false;
        }

        id<MTLCommandBuffer> command_buffer =
            [state.command_queue commandBuffer];
        if (command_buffer == nil) {
            error_message = "failed to create Metal upload command buffer";
            return false;
        }

        id<MTLComputeCommandEncoder> encoder =
            [command_buffer computeCommandEncoder];
        if (encoder == nil) {
            error_message = "failed to create Metal upload command encoder";
            return false;
        }

        NSMutableArray<id<MTLBuffer>>* stripe_buffers = [NSMutableArray array];
        [encoder setComputePipelineState:state.upload_pipeline];
        [encoder setTexture:texture atIndex:0];

        const MTLSize threads_per_group = MTLSizeMake(16, 16, 1);
        for (uint32_t stripe_index = 0; stripe_index < stripe_plan.stripe_count;
             ++stripe_index) {
            const uint32_t stripe_y = stripe_index * stripe_plan.stripe_rows;
            const uint32_t stripe_height
                = std::min(stripe_plan.stripe_rows,
                           static_cast<uint32_t>(image.height) - stripe_y);
            const size_t stripe_offset = static_cast<size_t>(stripe_y)
                                         * row_pitch_bytes;
            const size_t stripe_bytes = static_cast<size_t>(stripe_height)
                                        * row_pitch_bytes;

            id<MTLBuffer> source_buffer = [state.device
                newBufferWithBytes:upload_ptr + stripe_offset
                            length:static_cast<NSUInteger>(stripe_bytes)
                           options:MTLResourceStorageModeShared];
            if (source_buffer == nil) {
                error_message
                    = stripe_plan.uses_multiple_stripes
                          ? "failed to create Metal striped upload buffer"
                          : "failed to create Metal source upload buffer";
                [encoder endEncoding];
                return false;
            }
            [stripe_buffers addObject:source_buffer];

            MetalUploadUniforms uniforms = {};
            uniforms.width               = static_cast<uint32_t>(image.width);
            uniforms.height              = stripe_height;
            uniforms.dst_y_offset        = stripe_y;
            uniforms.row_pitch_bytes = static_cast<uint32_t>(row_pitch_bytes);
            uniforms.pixel_stride_bytes = static_cast<uint32_t>(
                pixel_stride_bytes);
            uniforms.channel_count = static_cast<uint32_t>(
                std::max(0, image.nchannels));
            uniforms.data_type = static_cast<uint32_t>(upload_type);

            [encoder setBuffer:source_buffer offset:0 atIndex:0];
            [encoder setBytes:&uniforms length:sizeof(uniforms) atIndex:1];

            const MTLSize threadgroups
                = MTLSizeMake((static_cast<NSUInteger>(image.width)
                               + threads_per_group.width - 1)
                                  / threads_per_group.width,
                              (static_cast<NSUInteger>(stripe_height)
                               + threads_per_group.height - 1)
                                  / threads_per_group.height,
                              1);
            [encoder dispatchThreadgroups:threadgroups
                    threadsPerThreadgroup:threads_per_group];
        }
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        if (command_buffer.status != MTLCommandBufferStatusCompleted) {
            error_message = "Metal source upload compute dispatch failed";
            return false;
        }

        error_message.clear();
        return true;
    }

    bool create_preview_texture(id<MTLDevice> device, int width, int height,
                                id<MTLTexture>& texture,
                                std::string& error_message)
    {
        texture = nil;
        if (device == nil || width <= 0 || height <= 0) {
            error_message = "invalid Metal preview texture parameters";
            return false;
        }
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
        descriptor.usage                 = MTLTextureUsageShaderRead
                           | MTLTextureUsageRenderTarget;
        texture = [device newTextureWithDescriptor:descriptor];
        if (texture == nil) {
            error_message = "failed to create Metal preview texture";
            return false;
        }
        error_message.clear();
        return true;
    }

    NSString* preview_shader_source()
    {
        std::string source = metal_preview_shader_preamble(
            metal_basic_preview_uniform_fields());
        source += metal_fullscreen_triangle_vertex_source("imivPreviewVertex");
        source += metal_preview_common_shader_functions();
        source += R"metal(

fragment float4 imivPreviewFragment(VertexOut in [[stage_in]],
                                    texture2d<float> source_texture [[texture(0)]],
                                    sampler source_sampler [[sampler(0)]],
                                    constant PreviewUniforms& uniforms [[buffer(0)]])
{
    // Metal's texture sampling origin differs from the existing preview UV
    // convention here only on the vertical axis. Normalize Y before applying
    // the shared orientation transform.
    float2 display_uv = float2(in.uv.x, 1.0 - in.uv.y);
    float2 src_uv = display_to_source_uv(display_uv, uniforms.orientation);
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

    if (uniforms.channel > 0 && uniforms.color_mode != 2
        && uniforms.color_mode != 4) {
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
        return [NSString stringWithUTF8String:source.c_str()];
    }

    bool create_preview_pipeline(RendererBackendState& state,
                                 std::string& error_message)
    {
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        if (!create_shader_library(state.device, preview_shader_source(),
                                   options, "Metal device is not initialized",
                                   "failed to compile Metal preview shader",
                                   state.preview_library, error_message)) {
            return false;
        }
        if (!create_render_pipeline_state(
                state.device, state.preview_library, "imivPreviewVertex",
                "imivPreviewFragment",
                "failed to create Metal preview shader functions",
                "failed to create Metal preview pipeline",
                state.preview_pipeline, error_message)) {
            return false;
        }
        state.linear_sampler = create_sampler_state(state.device,
                                                    OcioInterpolation::Linear);
        state.nearest_sampler = create_sampler_state(
            state.device, OcioInterpolation::Nearest);

        if (state.linear_sampler == nil || state.nearest_sampler == nil) {
            error_message = "failed to create Metal preview samplers";
            return false;
        }

        error_message.clear();
        return true;
    }

    bool build_ocio_preview_shader_source(const OcioShaderRuntime& runtime,
                                          std::string& shader_source,
                                          std::string& error_message)
    {
        shader_source.clear();
        error_message.clear();
        if (runtime.shader_desc == nullptr || !runtime.blueprint.enabled
            || runtime.blueprint.shader_text.empty()) {
            error_message = "Metal OCIO shader blueprint is invalid";
            return false;
        }

        std::ostringstream uniforms_struct;
        std::ostringstream uniform_bindings;
        std::ostringstream texture_bindings;
        std::ostringstream texture_call_params;
        std::ostringstream uniform_call_params;
        std::ostringstream call_params;
        bool has_uniform_struct        = false;
        bool uniform_need_separator    = false;
        bool texture_need_separator    = false;
        NSUInteger vector_buffer_index = 2;

        const unsigned num_uniforms = runtime.shader_desc->getNumUniforms();
        for (unsigned idx = 0; idx < num_uniforms; ++idx) {
            OCIO::GpuShaderDesc::UniformData data;
            const char* uniform_name = runtime.shader_desc->getUniform(idx,
                                                                       data);
            if (uniform_name == nullptr || uniform_name[0] == '\0') {
                error_message = "Metal OCIO uniform name is missing";
                return false;
            }

            switch (data.m_type) {
            case OCIO::UNIFORM_DOUBLE:
                uniforms_struct << "    float " << uniform_name << ";\n";
                if (uniform_need_separator)
                    uniform_call_params << ", ";
                uniform_call_params << "ocioUniformData." << uniform_name;
                uniform_need_separator = true;
                has_uniform_struct     = true;
                break;
            case OCIO::UNIFORM_BOOL:
                uniforms_struct << "    int " << uniform_name << ";\n";
                if (uniform_need_separator)
                    uniform_call_params << ", ";
                uniform_call_params << "(ocioUniformData." << uniform_name
                                    << " != 0)";
                uniform_need_separator = true;
                has_uniform_struct     = true;
                break;
            case OCIO::UNIFORM_FLOAT3:
                uniforms_struct << "    float4 " << uniform_name << ";\n";
                if (uniform_need_separator)
                    uniform_call_params << ", ";
                uniform_call_params << "ocioUniformData." << uniform_name
                                    << ".xyz";
                uniform_need_separator = true;
                has_uniform_struct     = true;
                break;
            case OCIO::UNIFORM_VECTOR_FLOAT:
                uniforms_struct << "    int "
                                << uniform_count_name(uniform_name) << ";\n";
                uniform_bindings << ",    constant float* " << uniform_name
                                 << " [[buffer(" << vector_buffer_index++
                                 << ")]]\n";
                if (uniform_need_separator)
                    uniform_call_params << ", ";
                uniform_call_params << uniform_name << ", ocioUniformData."
                                    << uniform_count_name(uniform_name);
                uniform_need_separator = true;
                has_uniform_struct     = true;
                break;
            case OCIO::UNIFORM_VECTOR_INT:
                uniforms_struct << "    int "
                                << uniform_count_name(uniform_name) << ";\n";
                uniform_bindings << ",    constant int* " << uniform_name
                                 << " [[buffer(" << vector_buffer_index++
                                 << ")]]\n";
                if (uniform_need_separator)
                    uniform_call_params << ", ";
                uniform_call_params << uniform_name << ", ocioUniformData."
                                    << uniform_count_name(uniform_name);
                uniform_need_separator = true;
                has_uniform_struct     = true;
                break;
            case OCIO::UNIFORM_UNKNOWN:
            default:
                error_message = "unsupported Metal OCIO uniform type";
                return false;
            }
        }

        NSUInteger texture_index = 1;
        for (unsigned idx = 0; idx < runtime.shader_desc->getNum3DTextures();
             ++idx) {
            const char* texture_name          = nullptr;
            const char* sampler_name          = nullptr;
            unsigned edge_len                 = 0;
            OCIO::Interpolation interpolation = OCIO::INTERP_DEFAULT;
            runtime.shader_desc->get3DTexture(idx, texture_name, sampler_name,
                                              edge_len, interpolation);
            if (texture_name == nullptr || sampler_name == nullptr) {
                error_message = "Metal OCIO 3D LUT binding is missing";
                return false;
            }
            texture_bindings << ",    texture3d<float> " << texture_name
                             << " [[texture(" << texture_index << ")]]\n"
                             << ",    sampler " << sampler_name << " [[sampler("
                             << texture_index << ")]]\n";
            if (texture_need_separator)
                texture_call_params << ", ";
            texture_call_params << texture_name << ", " << sampler_name;
            texture_need_separator = true;
            ++texture_index;
        }

        for (unsigned idx = 0; idx < runtime.shader_desc->getNumTextures();
             ++idx) {
            const char* texture_name = nullptr;
            const char* sampler_name = nullptr;
            unsigned width           = 0;
            unsigned height          = 0;
            OCIO::GpuShaderDesc::TextureType channel
                = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
            OCIO::GpuShaderCreator::TextureDimensions dimensions
                = OCIO::GpuShaderCreator::TextureDimensions::TEXTURE_2D;
            OCIO::Interpolation interpolation = OCIO::INTERP_DEFAULT;
            runtime.shader_desc->getTexture(idx, texture_name, sampler_name,
                                            width, height, channel, dimensions,
                                            interpolation);
            if (texture_name == nullptr || sampler_name == nullptr) {
                error_message = "Metal OCIO LUT binding is missing";
                return false;
            }
            texture_bindings
                << ",    "
                << (dimensions
                            == OCIO::GpuShaderCreator::TextureDimensions::TEXTURE_2D
                        ? "texture2d<float> "
                        : "texture1d<float> ")
                << texture_name << " [[texture(" << texture_index << ")]]\n"
                << ",    sampler " << sampler_name << " [[sampler("
                << texture_index << ")]]\n";
            if (texture_need_separator)
                texture_call_params << ", ";
            texture_call_params << texture_name << ", " << sampler_name;
            texture_need_separator = true;
            ++texture_index;
        }

        if (!texture_call_params.str().empty()) {
            call_params << texture_call_params.str();
        }
        if (!uniform_call_params.str().empty()) {
            if (!call_params.str().empty())
                call_params << ", ";
            call_params << uniform_call_params.str();
        }

        if (!call_params.str().empty())
            call_params << ", ";
        call_params << "rgba";

        std::ostringstream source;
        source << metal_preview_shader_preamble(
            metal_ocio_preview_uniform_fields());

        if (has_uniform_struct) {
            source << "\nstruct OcioUniformData {\n"
                   << uniforms_struct.str() << "};\n";
        }

        source << metal_fullscreen_triangle_vertex_source(
            "imivOcioPreviewVertex");
        source << metal_preview_common_shader_functions();

        source << runtime.blueprint.shader_text << "\n";
        source << "fragment float4 imivOcioPreviewFragment(\n"
               << "    VertexOut in [[stage_in]],\n"
               << "    texture2d<float> source_texture [[texture(0)]],\n"
               << "    sampler source_sampler [[sampler(0)]],\n"
               << "    constant PreviewUniforms& preview [[buffer(0)]]\n";
        if (has_uniform_struct) {
            source
                << ",    constant OcioUniformData& ocioUniformData [[buffer(1)]]\n";
        }
        source << uniform_bindings.str();
        source << texture_bindings.str();
        source
            << ")\n{\n"
            << "    float2 display_uv = float2(in.uv.x, 1.0 - in.uv.y);\n"
            << "    float2 src_uv = display_to_source_uv(display_uv, preview.orientation);\n"
            << "    float4 rgba = source_texture.sample(source_sampler, src_uv);\n"
            << "    rgba.rgb += float3(preview.offset);\n"
            << "    if (preview.color_mode == 1) {\n"
            << "        rgba.a = 1.0;\n"
            << "    } else if (preview.color_mode == 2) {\n"
            << "        float value = selected_channel(rgba, preview.channel);\n"
            << "        rgba = float4(value, value, value, 1.0);\n"
            << "    } else if (preview.color_mode == 3) {\n"
            << "        float value = dot(rgba.rgb, float3(0.2126, 0.7152, 0.0722));\n"
            << "        rgba = float4(value, value, value, 1.0);\n"
            << "    } else if (preview.color_mode == 4) {\n"
            << "        float value = selected_channel(rgba, preview.channel);\n"
            << "        rgba = float4(heatmap(value), 1.0);\n"
            << "    }\n"
            << "    if (preview.channel > 0 && preview.color_mode != 2 && preview.color_mode != 4) {\n"
            << "        float value = selected_channel(rgba, preview.channel);\n"
            << "        rgba = float4(value, value, value, 1.0);\n"
            << "    }\n"
            << "    if (preview.input_channels == 1 && preview.color_mode <= 1) {\n"
            << "        rgba = float4(rgba.rrr, 1.0);\n"
            << "    } else if (preview.input_channels == 2 && preview.color_mode == 0) {\n"
            << "        rgba = float4(rgba.rrr, rgba.a);\n"
            << "    } else if (preview.input_channels == 2 && preview.color_mode == 1) {\n"
            << "        rgba = float4(rgba.rrr, 1.0);\n"
            << "    }\n"
            << "    return " << runtime.blueprint.function_name << "("
            << call_params.str() << ");\n"
            << "}\n";

        shader_source = source.str();
        return true;
    }

    bool upload_ocio_texture(id<MTLDevice> device,
                             const OcioTextureBlueprint& blueprint,
                             NSUInteger texture_index,
                             MetalOcioTextureBinding& binding,
                             std::string& error_message)
    {
        binding               = {};
        binding.texture_name  = blueprint.texture_name;
        binding.sampler_name  = blueprint.sampler_name;
        binding.texture_index = texture_index;
        binding.sampler_index = texture_index;
        if (!create_ocio_texture(device, blueprint, binding.texture,
                                 error_message))
            return false;
        binding.sampler = create_sampler_state(device, blueprint.interpolation);
        if (binding.sampler == nil) {
            binding.texture = nil;
            error_message   = "failed to create Metal OCIO sampler state";
            return false;
        }
        return true;
    }

    bool ensure_ocio_preview_program(RendererBackendState& state,
                                     const PlaceholderUiState& ui_state,
                                     const LoadedImage* image,
                                     std::string& error_message)
    {
        if (!ensure_ocio_shader_runtime_metal(ui_state, image,
                                              state.ocio_preview.runtime,
                                              error_message)) {
            return false;
        }
        if (state.ocio_preview.runtime == nullptr
            || state.ocio_preview.runtime->shader_desc == nullptr) {
            error_message = "Metal OCIO runtime is not initialized";
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

        std::string shader_source;
        if (!build_ocio_preview_shader_source(*state.ocio_preview.runtime,
                                              shader_source, error_message)) {
            return false;
        }

        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        if (@available(macOS 10.13, *))
            [options setLanguageVersion:MTLLanguageVersion2_0];
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) \
    && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
        if (@available(macOS 15.0, *)) {
            [options setMathMode:MTLMathModeSafe];
        } else {
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [options setFastMathEnabled:NO];
#    pragma clang diagnostic pop
        }
#else
        [options setFastMathEnabled:NO];
#endif
        NSString* shader_source_ns
            = [NSString stringWithUTF8String:shader_source.c_str()];
        if (!create_shader_library(state.device, shader_source_ns, options,
                                   "Metal device is not initialized",
                                   "failed to compile Metal OCIO shader",
                                   state.ocio_preview.library,
                                   error_message)) {
            return false;
        }
        if (!create_render_pipeline_state(
                state.device, state.ocio_preview.library,
                "imivOcioPreviewVertex", "imivOcioPreviewFragment",
                "failed to create Metal OCIO shader functions",
                "failed to create Metal OCIO pipeline",
                state.ocio_preview.pipeline, error_message)) {
            return false;
        }

        state.ocio_preview.textures.clear();
        NSUInteger texture_index = 1;
        for (const OcioTextureBlueprint& texture : blueprint.textures) {
            if (texture.dimensions != OcioTextureDimensions::Tex3D)
                continue;
            MetalOcioTextureBinding binding;
            if (!upload_ocio_texture(state.device, texture, texture_index++,
                                     binding, error_message)) {
                destroy_ocio_preview_resources(state.ocio_preview);
                return false;
            }
            state.ocio_preview.textures.push_back(std::move(binding));
        }
        for (const OcioTextureBlueprint& texture : blueprint.textures) {
            if (texture.dimensions == OcioTextureDimensions::Tex3D)
                continue;
            MetalOcioTextureBinding binding;
            if (!upload_ocio_texture(state.device, texture, texture_index++,
                                     binding, error_message)) {
                destroy_ocio_preview_resources(state.ocio_preview);
                return false;
            }
            state.ocio_preview.textures.push_back(std::move(binding));
        }

        if (!build_ocio_vector_uniform_bindings(
                *state.ocio_preview.runtime, state.ocio_preview.vector_uniforms,
                error_message)) {
            destroy_ocio_preview_resources(state.ocio_preview);
            return false;
        }

        state.ocio_preview.shader_cache_id = blueprint.shader_cache_id;
        state.ocio_preview.ready           = true;
        error_message.clear();
        return true;
    }

    bool render_ocio_preview_texture(RendererBackendState& state,
                                     RendererTextureBackendState& texture_state,
                                     id<MTLTexture> target_texture,
                                     id<MTLSamplerState> source_sampler,
                                     const PreviewControls& controls,
                                     std::string& error_message)
    {
        if (state.command_queue == nil || state.ocio_preview.pipeline == nil
            || source_sampler == nil || texture_state.source_texture == nil
            || target_texture == nil || state.ocio_preview.runtime == nullptr) {
            error_message = "Metal OCIO preview state is not initialized";
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

        MetalPreviewUniforms preview_uniforms;
        preview_uniforms.offset         = controls.offset;
        preview_uniforms.color_mode     = controls.color_mode;
        preview_uniforms.channel        = controls.channel;
        preview_uniforms.input_channels = texture_state.input_channels;
        preview_uniforms.orientation    = controls.orientation;

        std::vector<unsigned char> scalar_uniform_bytes;
        if (!build_ocio_scalar_uniform_bytes(*state.ocio_preview.runtime,
                                             scalar_uniform_bytes,
                                             error_message)) {
            return false;
        }
        if (!build_ocio_vector_uniform_bindings(
                *state.ocio_preview.runtime, state.ocio_preview.vector_uniforms,
                error_message)) {
            return false;
        }

        id<MTLCommandBuffer> command_buffer =
            [state.command_queue commandBuffer];
        if (command_buffer == nil) {
            error_message = "failed to create Metal OCIO command buffer";
            return false;
        }

        MTLRenderPassDescriptor* pass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture     = target_texture;
        pass.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> encoder = [command_buffer
            renderCommandEncoderWithDescriptor:pass];
        if (encoder == nil) {
            error_message = "failed to create Metal OCIO encoder";
            return false;
        }

        [encoder setRenderPipelineState:state.ocio_preview.pipeline];
        [encoder setFragmentTexture:texture_state.source_texture atIndex:0];
        [encoder setFragmentSamplerState:source_sampler atIndex:0];
        [encoder setFragmentBytes:&preview_uniforms
                           length:sizeof(preview_uniforms)
                          atIndex:0];
        if (!scalar_uniform_bytes.empty()) {
            [encoder setFragmentBytes:scalar_uniform_bytes.data()
                               length:scalar_uniform_bytes.size()
                              atIndex:1];
        }

        for (const MetalOcioVectorUniformBinding& binding :
             state.ocio_preview.vector_uniforms) {
            [encoder setFragmentBytes:binding.bytes.data()
                               length:binding.bytes.size()
                              atIndex:binding.buffer_index];
        }
        for (const MetalOcioTextureBinding& binding :
             state.ocio_preview.textures) {
            [encoder setFragmentTexture:binding.texture
                                atIndex:binding.texture_index];
            [encoder setFragmentSamplerState:binding.sampler
                                     atIndex:binding.sampler_index];
        }

        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:3];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        if (command_buffer.status != MTLCommandBufferStatusCompleted) {
            error_message = "Metal OCIO preview render failed";
            return false;
        }
        error_message.clear();
        return true;
    }

    bool render_preview_texture(RendererBackendState& state,
                                RendererTextureBackendState& texture_state,
                                id<MTLTexture> target_texture,
                                id<MTLSamplerState> sampler_state,
                                const PreviewControls& controls,
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

        id<MTLCommandBuffer> command_buffer =
            [state.command_queue commandBuffer];
        if (command_buffer == nil) {
            error_message = "failed to create Metal preview command buffer";
            return false;
        }

        MTLRenderPassDescriptor* pass =
            [MTLRenderPassDescriptor renderPassDescriptor];
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
        [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
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

    bool metal_get_viewer_texture_refs(const ViewerState& viewer,
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

        ImTextureID main_texture_id = ui_state.linear_interpolation != 0
                                          ? state->preview_linear_tex_id
                                          : state->preview_nearest_tex_id;
        if (main_texture_id == ImTextureID_Invalid)
            main_texture_id = state->preview_linear_tex_id;
        if (main_texture_id != ImTextureID_Invalid) {
            main_texture_ref = ImTextureRef(main_texture_id);
            has_main_texture = true;
        }

        ImTextureID closeup_texture_id = state->preview_nearest_tex_id;
        if (closeup_texture_id == ImTextureID_Invalid)
            closeup_texture_id = state->preview_linear_tex_id;
        if (closeup_texture_id != ImTextureID_Invalid) {
            closeup_texture_ref = ImTextureRef(closeup_texture_id);
            has_closeup_texture = true;
        }
        return has_main_texture || has_closeup_texture;
    }

    bool metal_texture_is_loading(const RendererTexture& texture)
    {
        return texture.backend != nullptr && !texture.preview_initialized;
    }

    bool metal_create_texture(RendererState& renderer_state,
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

        if (!create_source_texture(state->device, image.width, image.height,
                                   texture_state->source_texture, error_message)
            || !upload_source_texture(*state, image,
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

        texture_state->preview_linear_tex_id
            = ImGui_ImplMetal_CreateUserTextureID(
                texture_state->preview_linear_texture, state->linear_sampler);
        texture_state->preview_nearest_tex_id
            = ImGui_ImplMetal_CreateUserTextureID(
                texture_state->preview_nearest_texture, state->nearest_sampler);
        if (texture_state->preview_linear_tex_id == ImTextureID_Invalid
            || texture_state->preview_nearest_tex_id == ImTextureID_Invalid) {
            if (texture_state->preview_linear_tex_id != ImTextureID_Invalid)
                ImGui_ImplMetal_DestroyUserTextureID(
                    texture_state->preview_linear_tex_id);
            if (texture_state->preview_nearest_tex_id != ImTextureID_Invalid)
                ImGui_ImplMetal_DestroyUserTextureID(
                    texture_state->preview_nearest_tex_id);
            texture_state->preview_linear_tex_id   = ImTextureID_Invalid;
            texture_state->preview_nearest_tex_id  = ImTextureID_Invalid;
            texture_state->source_texture          = nil;
            texture_state->preview_linear_texture  = nil;
            texture_state->preview_nearest_texture = nil;
            error_message = "failed to create Metal ImGui texture bindings";
            delete texture_state;
            return false;
        }

        texture_state->width          = image.width;
        texture_state->height         = image.height;
        texture_state->input_channels = image.nchannels;
        texture_state->preview_dirty  = true;

        texture.backend = reinterpret_cast<::Imiv::RendererTextureBackendState*>(
            texture_state);
        texture.preview_initialized = false;
        error_message.clear();
        return true;
    }

    void metal_destroy_texture(RendererState& renderer_state,
                               RendererTexture& texture)
    {
        (void)renderer_state;
        RendererTextureBackendState* state = texture_backend_state(texture);
        if (state == nullptr) {
            texture.preview_initialized = false;
            return;
        }
        if (state->preview_linear_tex_id != ImTextureID_Invalid) {
            ImGui_ImplMetal_DestroyUserTextureID(state->preview_linear_tex_id);
            state->preview_linear_tex_id = ImTextureID_Invalid;
        }
        if (state->preview_nearest_tex_id != ImTextureID_Invalid) {
            ImGui_ImplMetal_DestroyUserTextureID(state->preview_nearest_tex_id);
            state->preview_nearest_tex_id = ImTextureID_Invalid;
        }
        state->source_texture          = nil;
        state->preview_linear_texture  = nil;
        state->preview_nearest_texture = nil;
        delete state;
        texture.backend             = nullptr;
        texture.preview_initialized = false;
    }

    bool metal_update_preview_texture(RendererState& renderer_state,
                                      RendererTexture& texture,
                                      const LoadedImage* image,
                                      const PlaceholderUiState& ui_state,
                                      const PreviewControls& controls,
                                      std::string& error_message)
    {
        RendererBackendState* renderer_backend = backend_state(renderer_state);
        RendererTextureBackendState* texture_state = texture_backend_state(
            texture);
        if (renderer_backend == nullptr || texture_state == nullptr) {
            error_message = "Metal preview state is not initialized";
            return false;
        }

        PreviewControls effective_controls = controls;
        if (!texture_state->preview_dirty && texture_state->preview_params_valid
            && preview_controls_equal(texture_state->last_preview_controls,
                                      effective_controls)
            && effective_controls.use_ocio == 0) {
            texture.preview_initialized = true;
            error_message.clear();
            return true;
        }

        bool used_ocio = false;
        if (effective_controls.use_ocio != 0) {
            std::string ocio_error;
            if (ensure_ocio_preview_program(*renderer_backend, ui_state, image,
                                            ocio_error)
                && render_ocio_preview_texture(
                    *renderer_backend, *texture_state,
                    texture_state->preview_linear_texture,
                    renderer_backend->linear_sampler, effective_controls,
                    ocio_error)
                && render_ocio_preview_texture(
                    *renderer_backend, *texture_state,
                    texture_state->preview_nearest_texture,
                    renderer_backend->nearest_sampler, effective_controls,
                    ocio_error)) {
                used_ocio = true;
            } else {
                if (!ocio_error.empty()) {
                    std::cerr << "imiv: Metal OCIO fallback: " << ocio_error
                              << "\n";
                }
                effective_controls.use_ocio = 0;
            }
        }

        if (!used_ocio
            && (!render_preview_texture(*renderer_backend, *texture_state,
                                        texture_state->preview_linear_texture,
                                        renderer_backend->linear_sampler,
                                        effective_controls, error_message)
                || !render_preview_texture(
                    *renderer_backend, *texture_state,
                    texture_state->preview_nearest_texture,
                    renderer_backend->nearest_sampler, effective_controls,
                    error_message))) {
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

    bool metal_quiesce_texture_preview_submission(RendererState& renderer_state,
                                                  RendererTexture& texture,
                                                  std::string& error_message)
    {
        (void)renderer_state;
        (void)texture;
        error_message.clear();
        return true;
    }

    bool metal_setup_instance(RendererState& renderer_state,
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

    bool metal_create_surface(RendererState& renderer_state, GLFWwindow* window,
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

    bool metal_setup_device(RendererState& renderer_state,
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
        if (!create_preview_pipeline(*state, error_message)
            || !create_upload_pipeline(*state, error_message))
            return false;
        error_message.clear();
        return true;
    }

    bool metal_setup_window(RendererState& renderer_state, int width,
                            int height, std::string& error_message)
    {
        RendererBackendState* state = backend_state(renderer_state);
        if (state == nullptr || state->window == nullptr
            || state->device == nil) {
            error_message = "Metal window/device is not initialized";
            return false;
        }
        NSWindow* ns_window = glfwGetCocoaWindow(state->window);
        if (ns_window == nil) {
            error_message = "failed to get Cocoa window from GLFW";
            return false;
        }
        state->layer                      = [CAMetalLayer layer];
        state->layer.device               = state->device;
        state->layer.pixelFormat          = MTLPixelFormatBGRA8Unorm;
        state->layer.framebufferOnly      = NO;
        ns_window.contentView.layer       = state->layer;
        ns_window.contentView.wantsLayer  = YES;
        state->render_pass                = [MTLRenderPassDescriptor new];
        renderer_state.framebuffer_width  = width;
        renderer_state.framebuffer_height = height;
        update_drawable_size(renderer_state);
        error_message.clear();
        return true;
    }

    void metal_destroy_surface(RendererState& renderer_state)
    {
        if (RendererBackendState* state = backend_state(renderer_state))
            state->window = nullptr;
    }

    void metal_cleanup_window(RendererState& renderer_state)
    {
        RendererBackendState* state = backend_state(renderer_state);
        if (state == nullptr)
            return;
        state->current_drawable       = nil;
        state->current_command_buffer = nil;
        state->current_encoder        = nil;
    }

    void metal_cleanup(RendererState& renderer_state)
    {
        if (RendererBackendState* state = backend_state(renderer_state)) {
            destroy_ocio_preview_program(state->ocio_preview);
            delete state;
        }
        renderer_state.backend = nullptr;
    }

    bool metal_wait_idle(RendererState& renderer_state,
                         std::string& error_message)
    {
        RendererBackendState* state = backend_state(renderer_state);
        if (state != nullptr && state->current_command_buffer != nil)
            [state->current_command_buffer waitUntilCompleted];
        error_message.clear();
        return true;
    }

    bool metal_imgui_init(RendererState& renderer_state,
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

    void metal_imgui_shutdown() { ImGui_ImplMetal_Shutdown(); }

    void metal_imgui_new_frame(RendererState& renderer_state)
    {
        RendererBackendState* state = backend_state(renderer_state);
        if (state == nullptr || state->layer == nil
            || state->render_pass == nil)
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

    bool metal_needs_main_window_resize(RendererState& renderer_state,
                                        int width, int height)
    {
        return renderer_state.framebuffer_width != width
               || renderer_state.framebuffer_height != height;
    }

    void metal_resize_main_window(RendererState& renderer_state, int width,
                                  int height)
    {
        renderer_state.framebuffer_width  = width;
        renderer_state.framebuffer_height = height;
        update_drawable_size(renderer_state);
    }

    void metal_set_main_clear_color(RendererState& renderer_state, float r,
                                    float g, float b, float a)
    {
        renderer_state.clear_color[0] = r;
        renderer_state.clear_color[1] = g;
        renderer_state.clear_color[2] = b;
        renderer_state.clear_color[3] = a;
    }

    void metal_prepare_platform_windows(RendererState& renderer_state)
    {
        (void)renderer_state;
    }

    void metal_finish_platform_windows(RendererState& renderer_state)
    {
        (void)renderer_state;
    }

    void metal_frame_render(RendererState& renderer_state,
                            ImDrawData* draw_data)
    {
        RendererBackendState* state = backend_state(renderer_state);
        if (state == nullptr || state->current_drawable == nil
            || state->command_queue == nil || state->render_pass == nil) {
            return;
        }
        state->current_command_buffer = [state->command_queue commandBuffer];
        state->current_encoder        = [state->current_command_buffer
            renderCommandEncoderWithDescriptor:state->render_pass];
        ImGui_ImplMetal_RenderDrawData(draw_data, state->current_command_buffer,
                                       state->current_encoder);
    }

    void metal_frame_present(RendererState& renderer_state)
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

    bool metal_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                              unsigned int* pixels, void* user_data)
    {
        (void)viewport_id;
        RendererState* renderer_state = reinterpret_cast<RendererState*>(
            user_data);
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

        const double scale_x    = (window_width > 0)
                                      ? static_cast<double>(texture_width)
                                         / static_cast<double>(window_width)
                                      : 1.0;
        const double scale_y    = (window_height > 0)
                                      ? static_cast<double>(texture_height)
                                         / static_cast<double>(window_height)
                                      : 1.0;
        const bool logical_rect = (window_width > 0 && window_height > 0
                                   && w > 0 && h > 0 && w <= window_width
                                   && h <= window_height
                                   && (std::abs(scale_x - 1.0) > 1.0e-3
                                       || std::abs(scale_y - 1.0) > 1.0e-3));

        const int src_x = logical_rect
                              ? static_cast<int>(std::lround(x * scale_x))
                              : x;
        const int src_y = logical_rect
                              ? static_cast<int>(std::lround(y * scale_y))
                              : y;
        const int src_w
            = logical_rect ? std::max(1, static_cast<int>(std::lround(
                                             static_cast<double>(w) * scale_x)))
                           : w;
        const int src_h
            = logical_rect ? std::max(1, static_cast<int>(std::lround(
                                             static_cast<double>(h) * scale_y)))
                           : h;
        if (src_x < 0 || src_y < 0 || src_x + src_w > texture_width
            || src_y + src_h > texture_height) {
            return false;
        }

        [state->current_encoder endEncoding];
        state->current_encoder = nil;

        const NSUInteger bytes_per_pixel = 4;
        const NSUInteger row_bytes
            = align_up(static_cast<NSUInteger>(src_w) * bytes_per_pixel, 256);
        const NSUInteger buffer_size = row_bytes
                                       * static_cast<NSUInteger>(src_h);

        id<MTLBuffer> readback_buffer = [state->device
            newBufferWithLength:buffer_size
                        options:MTLResourceStorageModeShared];
        if (readback_buffer == nil)
            return false;

        id<MTLBlitCommandEncoder> blit =
            [state->current_command_buffer blitCommandEncoder];
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

        if (state->current_command_buffer.status
            != MTLCommandBufferStatusCompleted)
            return false;

        const unsigned char* src_bytes = static_cast<const unsigned char*>(
            [readback_buffer contents]);
        if (src_bytes == nullptr)
            return false;

        unsigned char* dst_bytes    = reinterpret_cast<unsigned char*>(pixels);
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
            const unsigned char* src_row = src_bytes
                                           + static_cast<size_t>(sample_row)
                                                 * static_cast<size_t>(
                                                     row_bytes);
            for (int col = 0; col < w; ++col) {
                const int sample_col = std::clamp(
                    static_cast<int>(std::floor((static_cast<double>(col) + 0.5)
                                                * sample_scale_x)),
                    0, src_w - 1);
                const unsigned char* src
                    = src_row + static_cast<size_t>(sample_col) * 4;
                unsigned char* dst = dst_row + static_cast<size_t>(col) * 4;
                dst[0]             = src[2];
                dst[1]             = src[1];
                dst[2]             = src[0];
                dst[3]             = src[3];
            }
        }

        state->current_command_buffer = nil;
        state->current_drawable       = nil;
        return true;
    }

    bool metal_probe_runtime_support(std::string& error_message)
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            error_message = "Metal device is unavailable";
            return false;
        }
        error_message.clear();
        return true;
    }

}  // namespace
}  // namespace Imiv

namespace Imiv {

const RendererBackendVTable k_metal_vtable = {
    BackendKind::Metal,
    metal_probe_runtime_support,
    metal_get_viewer_texture_refs,
    metal_texture_is_loading,
    metal_create_texture,
    metal_destroy_texture,
    metal_update_preview_texture,
    metal_quiesce_texture_preview_submission,
    metal_setup_instance,
    metal_setup_device,
    metal_setup_window,
    metal_create_surface,
    metal_destroy_surface,
    metal_cleanup_window,
    metal_cleanup,
    metal_wait_idle,
    metal_imgui_init,
    metal_imgui_shutdown,
    metal_imgui_new_frame,
    metal_needs_main_window_resize,
    metal_resize_main_window,
    metal_set_main_clear_color,
    metal_prepare_platform_windows,
    metal_finish_platform_windows,
    metal_frame_render,
    metal_frame_present,
    metal_screen_capture,
};

const RendererBackendVTable*
renderer_backend_metal_vtable()
{
    return &k_metal_vtable;
}

}  // namespace Imiv
