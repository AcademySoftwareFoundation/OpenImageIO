// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

#include <OpenColorIO/OpenColorIO.h>
#include <OpenColorIO/OpenColorTransforms.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace OCIO = OCIO_NAMESPACE;

namespace Imiv {

enum class OcioShaderTarget : uint8_t { Vulkan = 0, OpenGL, Metal };

enum class OcioUniformType : uint8_t {
    Unknown = 0,
    Double,
    Bool,
    Float3,
    VectorFloat,
    VectorInt
};

enum class OcioTextureChannel : uint8_t { Red = 0, RGB };

enum class OcioTextureDimensions : uint8_t { Tex1D = 0, Tex2D, Tex3D };

enum class OcioInterpolation : uint8_t { Unknown = 0, Nearest, Linear };

struct OcioUniformBlueprint {
    std::string name;
    OcioUniformType type = OcioUniformType::Unknown;
    size_t buffer_offset = 0;
};

struct OcioTextureBlueprint {
    std::string texture_name;
    std::string sampler_name;
    unsigned shader_binding          = 0;
    unsigned width                   = 0;
    unsigned height                  = 0;
    unsigned depth                   = 0;
    OcioTextureChannel channel       = OcioTextureChannel::RGB;
    OcioTextureDimensions dimensions = OcioTextureDimensions::Tex2D;
    OcioInterpolation interpolation  = OcioInterpolation::Unknown;
    std::vector<float> values;
};

struct OcioConfigSelection {
    OcioConfigSource requested_source = OcioConfigSource::Global;
    OcioConfigSource resolved_source  = OcioConfigSource::Global;
    bool fallback_applied             = false;
    std::string env_value;
    std::string resolved_path;
    std::string selection_key;
};

struct OcioShaderBlueprint {
    bool enabled                   = false;
    bool valid                     = false;
    bool has_dynamic_exposure      = false;
    bool has_dynamic_gamma         = false;
    float wrapper_offset           = 0.0f;
    unsigned descriptor_set_index  = 1;
    unsigned texture_binding_start = 1;
    size_t uniform_buffer_size     = 0;
    std::string input_color_space;
    std::string display;
    std::string view;
    std::string config_selection_key;
    std::string processor_cache_id;
    std::string shader_cache_id;
    std::string function_name   = "imivOcioDisplay";
    std::string resource_prefix = "imiv_ocio_";
    std::string shader_text;
    std::vector<OcioUniformBlueprint> uniforms;
    std::vector<OcioTextureBlueprint> textures;
};

struct OcioShaderRuntime {
    bool enabled            = false;
    OcioShaderTarget target = OcioShaderTarget::Vulkan;
    OcioShaderBlueprint blueprint;
    OCIO::ConstConfigRcPtr config;
    OCIO::ConstProcessorRcPtr processor;
    OCIO::ConstGPUProcessorRcPtr gpu_processor;
    OCIO::GpuShaderDescRcPtr shader_desc;
    OCIO::DynamicPropertyDoubleRcPtr exposure_property;
    OCIO::DynamicPropertyDoubleRcPtr gamma_property;
};

void
reset_ocio_shader_blueprint(OcioShaderBlueprint& blueprint);
const char*
ocio_config_source_name(OcioConfigSource source);
void
resolve_ocio_config_selection(const PlaceholderUiState& ui_state,
                              OcioConfigSelection& selection);

void
destroy_ocio_shader_runtime(OcioShaderRuntime*& runtime);

bool
build_ocio_shader_blueprint(const PlaceholderUiState& ui_state,
                            const LoadedImage* image,
                            OcioShaderBlueprint& blueprint,
                            std::string& error_message);
bool
ensure_ocio_shader_runtime(const PlaceholderUiState& ui_state,
                           const LoadedImage* image,
                           OcioShaderRuntime*& runtime,
                           std::string& error_message);
bool
ensure_ocio_shader_runtime_glsl(const PlaceholderUiState& ui_state,
                                const LoadedImage* image,
                                OcioShaderRuntime*& runtime,
                                std::string& error_message);
bool
ensure_ocio_shader_runtime_metal(const PlaceholderUiState& ui_state,
                                 const LoadedImage* image,
                                 OcioShaderRuntime*& runtime,
                                 std::string& error_message);
bool
build_ocio_uniform_buffer(OcioShaderRuntime& runtime,
                          const RendererPreviewControls& controls,
                          std::vector<unsigned char>& uniform_bytes,
                          std::string& error_message);
#if defined(IMIV_WITH_VULKAN)
bool
build_ocio_uniform_buffer(OcioShaderRuntime& runtime,
                          const PreviewControls& controls,
                          std::vector<unsigned char>& uniform_bytes,
                          std::string& error_message);
#endif
bool
query_ocio_menu_data(const PlaceholderUiState& ui_state,
                     std::vector<std::string>& image_color_spaces,
                     std::vector<std::string>& displays,
                     std::vector<std::string>& views,
                     std::string& resolved_display, std::string& resolved_view,
                     std::string& error_message);
bool
build_ocio_preview_fragment_source(const OcioShaderBlueprint& blueprint,
                                   std::string& shader_source,
                                   std::string& error_message);
bool
compile_ocio_preview_fragment_spirv(const OcioShaderBlueprint& blueprint,
                                    std::vector<uint32_t>& spirv_words,
                                    std::string& error_message);
bool
preflight_ocio_runtime_shader(const PlaceholderUiState& ui_state,
                              const LoadedImage* image,
                              std::string& error_message);
bool
preflight_ocio_runtime_shader_glsl(const PlaceholderUiState& ui_state,
                                   const LoadedImage* image,
                                   std::string& error_message);
bool
preflight_ocio_runtime_shader_metal(const PlaceholderUiState& ui_state,
                                    const LoadedImage* image,
                                    std::string& error_message);

}  // namespace Imiv
