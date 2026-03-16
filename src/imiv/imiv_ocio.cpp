// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ocio.h"
#include "imiv_shader_compile.h"

#include <cstring>
#include <filesystem>
#include <string_view>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

namespace Imiv {

namespace {

    std::string builtin_ocio_config_path() { return "ocio://default"; }

    OcioConfigSource ocio_config_source_from_int(int value)
    {
        value = std::clamp(value, static_cast<int>(OcioConfigSource::Global),
                           static_cast<int>(OcioConfigSource::User));
        return static_cast<OcioConfigSource>(value);
    }

    bool ocio_source_string_is_usable(std::string_view value)
    {
        const std::string trimmed = std::string(OIIO::Strutil::strip(value));
        if (trimmed.empty())
            return false;
        if (OIIO::Strutil::istarts_with(trimmed, "ocio://"))
            return true;
        std::error_code ec;
        return std::filesystem::exists(std::filesystem::path(trimmed), ec)
               && !ec;
    }

    OcioUniformType map_ocio_uniform_type(OCIO::UniformDataType type)
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

    OcioTextureChannel
    map_ocio_texture_channel(OCIO::GpuShaderDesc::TextureType type)
    {
        if (type == OCIO::GpuShaderDesc::TEXTURE_RED_CHANNEL)
            return OcioTextureChannel::Red;
        return OcioTextureChannel::RGB;
    }

    OcioTextureDimensions map_ocio_texture_dimensions(
        OCIO::GpuShaderCreator::TextureDimensions dimensions, unsigned height)
    {
        if (dimensions == OCIO::GpuShaderCreator::TextureDimensions::TEXTURE_1D)
            return OcioTextureDimensions::Tex1D;
        if (height <= 1)
            return OcioTextureDimensions::Tex1D;
        return OcioTextureDimensions::Tex2D;
    }

    OcioInterpolation map_ocio_interpolation(OCIO::Interpolation interpolation)
    {
        if (interpolation == OCIO::INTERP_NEAREST)
            return OcioInterpolation::Nearest;
        if (interpolation == OCIO::INTERP_LINEAR)
            return OcioInterpolation::Linear;
        return OcioInterpolation::Unknown;
    }

    std::string
    resolve_scene_linear_color_space(const OCIO::ConstConfigRcPtr& config)
    {
        if (!config)
            return "scene_linear";
        OCIO::ConstColorSpaceRcPtr color_space = config->getColorSpace(
            "scene_linear");
        if (color_space != nullptr && color_space->getName() != nullptr)
            return color_space->getName();
        const int num_color_spaces = config->getNumColorSpaces();
        if (num_color_spaces > 0) {
            const char* fallback = config->getColorSpaceNameByIndex(0);
            if (fallback != nullptr && fallback[0] != '\0')
                return fallback;
        }
        return "scene_linear";
    }

    bool load_ocio_config_from_selection(const OcioConfigSelection& selection,
                                         OCIO::ConstConfigRcPtr& config,
                                         std::string& error_message)
    {
        error_message.clear();
        config.reset();
        try {
            if (selection.resolved_path.empty()) {
                error_message = "OCIO config path is empty";
                return false;
            }
            config = OCIO::Config::CreateFromFile(
                selection.resolved_path.c_str());
        } catch (const OCIO::Exception& e) {
            error_message = e.what();
            return false;
        }

        if (!config) {
            error_message = "OCIO config is unavailable";
            return false;
        }
        return true;
    }

    std::string match_config_color_space(const OCIO::ConstConfigRcPtr& config,
                                         std::string_view candidate)
    {
        const std::string trimmed = std::string(
            OIIO::Strutil::strip(candidate));
        if (trimmed.empty() || !config)
            return std::string();

        OCIO::ConstColorSpaceRcPtr exact = config->getColorSpace(
            trimmed.c_str());
        if (exact != nullptr && exact->getName() != nullptr)
            return exact->getName();

        const int num_color_spaces = config->getNumColorSpaces();
        for (int i = 0; i < num_color_spaces; ++i) {
            const char* config_name = config->getColorSpaceNameByIndex(i);
            if (config_name == nullptr || config_name[0] == '\0')
                continue;
            if (OIIO::equivalent_colorspace(trimmed, config_name))
                return config_name;
        }
        return std::string();
    }

    bool config_has_display(const OCIO::ConstConfigRcPtr& config,
                            std::string_view display_name)
    {
        if (!config)
            return false;
        const std::string trimmed = std::string(
            OIIO::Strutil::strip(display_name));
        if (trimmed.empty())
            return false;
        const int num_displays = config->getNumDisplays();
        for (int i = 0; i < num_displays; ++i) {
            const char* config_display = config->getDisplay(i);
            if (config_display == nullptr || config_display[0] == '\0')
                continue;
            if (trimmed == config_display)
                return true;
        }
        return false;
    }

    bool config_has_view(const OCIO::ConstConfigRcPtr& config,
                         std::string_view display_name,
                         std::string_view view_name)
    {
        if (!config)
            return false;
        const std::string display = std::string(
            OIIO::Strutil::strip(display_name));
        const std::string view = std::string(OIIO::Strutil::strip(view_name));
        if (display.empty() || view.empty())
            return false;
        const int num_views = config->getNumViews(display.c_str());
        for (int i = 0; i < num_views; ++i) {
            const char* config_view = config->getView(display.c_str(), i);
            if (config_view == nullptr || config_view[0] == '\0')
                continue;
            if (view == config_view)
                return true;
        }
        return false;
    }

    std::string
    resolve_default_display_name(const OCIO::ConstConfigRcPtr& config)
    {
        if (!config)
            return {};
        const char* display_name = config->getDefaultDisplay();
        if (display_name != nullptr && display_name[0] != '\0')
            return display_name;
        const int num_displays = config->getNumDisplays();
        for (int i = 0; i < num_displays; ++i) {
            const char* fallback_name = config->getDisplay(i);
            if (fallback_name != nullptr && fallback_name[0] != '\0')
                return fallback_name;
        }
        return {};
    }

    std::string resolve_default_view_name(const OCIO::ConstConfigRcPtr& config,
                                          std::string_view display_name)
    {
        if (!config)
            return {};
        const std::string display = std::string(
            OIIO::Strutil::strip(display_name));
        if (display.empty())
            return {};
        const char* view_name = config->getDefaultView(display.c_str());
        if (view_name != nullptr && view_name[0] != '\0')
            return view_name;
        const int num_views = config->getNumViews(display.c_str());
        for (int i = 0; i < num_views; ++i) {
            const char* fallback_name = config->getView(display.c_str(), i);
            if (fallback_name != nullptr && fallback_name[0] != '\0')
                return fallback_name;
        }
        return {};
    }

    std::string resolve_input_color_space(const PlaceholderUiState& ui_state,
                                          const LoadedImage* image,
                                          const OCIO::ConstConfigRcPtr& config)
    {
        if (!ui_state.ocio_image_color_space.empty()
            && ui_state.ocio_image_color_space != "auto") {
            const std::string matched
                = match_config_color_space(config,
                                           ui_state.ocio_image_color_space);
            if (!matched.empty())
                return matched;
        }

        if (image != nullptr) {
            const std::string matched
                = match_config_color_space(config, image->metadata_color_space);
            if (!matched.empty())
                return matched;
        }

        return resolve_scene_linear_color_space(config);
    }

    std::string resolve_display_name(const PlaceholderUiState& ui_state,
                                     const OCIO::ConstConfigRcPtr& config)
    {
        if (!ui_state.ocio_display.empty() && ui_state.ocio_display != "default"
            && config_has_display(config, ui_state.ocio_display)) {
            return ui_state.ocio_display;
        }
        return resolve_default_display_name(config);
    }

    std::string resolve_view_name(const PlaceholderUiState& ui_state,
                                  const OCIO::ConstConfigRcPtr& config,
                                  const std::string& display_name)
    {
        if (!ui_state.ocio_view.empty() && ui_state.ocio_view != "default"
            && config_has_view(config, display_name, ui_state.ocio_view)) {
            return ui_state.ocio_view;
        }
        return resolve_default_view_name(config, display_name);
    }

    bool write_uniform_bytes(std::vector<unsigned char>& uniform_bytes,
                             size_t offset, const void* src, size_t size,
                             std::string& error_message)
    {
        if (src == nullptr || size == 0)
            return true;
        if (offset + size > uniform_bytes.size()) {
            error_message = OIIO::Strutil::fmt::format(
                "OCIO uniform write overflow at offset {} size {} buffer {}",
                offset, size, uniform_bytes.size());
            return false;
        }
        std::memcpy(uniform_bytes.data() + offset, src, size);
        return true;
    }

    bool
    populate_ocio_shader_blueprint(const PlaceholderUiState& ui_state,
                                   const OCIO::ConstProcessorRcPtr& processor,
                                   const OCIO::GpuShaderDescRcPtr& shader_desc,
                                   OcioShaderBlueprint& blueprint,
                                   std::string& error_message)
    {
        blueprint.enabled              = ui_state.use_ocio;
        blueprint.valid                = true;
        blueprint.wrapper_offset       = ui_state.offset;
        blueprint.uniform_buffer_size  = shader_desc->getUniformBufferSize();
        blueprint.processor_cache_id   = processor->getCacheID()
                                             ? processor->getCacheID()
                                             : std::string();
        blueprint.shader_cache_id      = shader_desc->getCacheID()
                                             ? shader_desc->getCacheID()
                                             : std::string();
        blueprint.shader_text          = shader_desc->getShaderText()
                                             ? shader_desc->getShaderText()
                                             : std::string();
        blueprint.has_dynamic_exposure = shader_desc->hasDynamicProperty(
            OCIO::DYNAMIC_PROPERTY_EXPOSURE);
        blueprint.has_dynamic_gamma = shader_desc->hasDynamicProperty(
            OCIO::DYNAMIC_PROPERTY_GAMMA);

        const unsigned num_uniforms = shader_desc->getNumUniforms();
        blueprint.uniforms.clear();
        blueprint.uniforms.reserve(num_uniforms);
        for (unsigned idx = 0; idx < num_uniforms; ++idx) {
            OCIO::GpuShaderDesc::UniformData data;
            const char* name = shader_desc->getUniform(idx, data);
            OcioUniformBlueprint uniform;
            if (name)
                uniform.name = name;
            uniform.type          = map_ocio_uniform_type(data.m_type);
            uniform.buffer_offset = data.m_bufferOffset;
            blueprint.uniforms.push_back(std::move(uniform));
        }

        const unsigned num_textures = shader_desc->getNumTextures();
        blueprint.textures.clear();
        blueprint.textures.reserve(num_textures
                                   + shader_desc->getNum3DTextures());
        for (unsigned idx = 0; idx < num_textures; ++idx) {
            const char* texture_name = nullptr;
            const char* sampler_name = nullptr;
            unsigned width           = 0;
            unsigned height          = 0;
            OCIO::GpuShaderDesc::TextureType channel
                = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
            OCIO::GpuShaderCreator::TextureDimensions dimensions
                = OCIO::GpuShaderCreator::TextureDimensions::TEXTURE_2D;
            OCIO::Interpolation interpolation = OCIO::INTERP_DEFAULT;
            shader_desc->getTexture(idx, texture_name, sampler_name, width,
                                    height, channel, dimensions, interpolation);
            const float* values = nullptr;
            shader_desc->getTextureValues(idx, values);

            OcioTextureBlueprint texture;
            if (texture_name)
                texture.texture_name = texture_name;
            if (sampler_name)
                texture.sampler_name = sampler_name;
            texture.shader_binding = shader_desc->getTextureShaderBindingIndex(
                idx);
            texture.width         = width;
            texture.height        = height;
            texture.depth         = 1;
            texture.channel       = map_ocio_texture_channel(channel);
            texture.dimensions    = map_ocio_texture_dimensions(dimensions,
                                                                height);
            texture.interpolation = map_ocio_interpolation(interpolation);
            if (values) {
                const size_t texel_count
                    = static_cast<size_t>(width) * static_cast<size_t>(height)
                      * static_cast<size_t>(
                          channel == OCIO::GpuShaderDesc::TEXTURE_RED_CHANNEL
                              ? 1
                              : 3);
                texture.values.assign(values, values + texel_count);
            }
            blueprint.textures.push_back(std::move(texture));
        }

        const unsigned num_textures_3d = shader_desc->getNum3DTextures();
        for (unsigned idx = 0; idx < num_textures_3d; ++idx) {
            const char* texture_name          = nullptr;
            const char* sampler_name          = nullptr;
            unsigned edge_len                 = 0;
            OCIO::Interpolation interpolation = OCIO::INTERP_DEFAULT;
            shader_desc->get3DTexture(idx, texture_name, sampler_name, edge_len,
                                      interpolation);
            const float* values = nullptr;
            shader_desc->get3DTextureValues(idx, values);

            OcioTextureBlueprint texture;
            if (texture_name)
                texture.texture_name = texture_name;
            if (sampler_name)
                texture.sampler_name = sampler_name;
            texture.shader_binding
                = shader_desc->get3DTextureShaderBindingIndex(idx);
            texture.width         = edge_len;
            texture.height        = edge_len;
            texture.depth         = edge_len;
            texture.channel       = OcioTextureChannel::RGB;
            texture.dimensions    = OcioTextureDimensions::Tex3D;
            texture.interpolation = map_ocio_interpolation(interpolation);
            if (values) {
                const size_t texel_count = static_cast<size_t>(edge_len)
                                           * static_cast<size_t>(edge_len)
                                           * static_cast<size_t>(edge_len) * 3u;
                texture.values.assign(values, values + texel_count);
            }
            blueprint.textures.push_back(std::move(texture));
        }

        if (blueprint.display.empty() || blueprint.view.empty()) {
            error_message = "OCIO display/view resolution failed";
            return false;
        }
        return true;
    }

    bool build_ocio_shader_runtime_internal(const PlaceholderUiState& ui_state,
                                            const LoadedImage* image,
                                            OcioShaderRuntime& runtime,
                                            std::string& error_message)
    {
        runtime                          = {};
        runtime.enabled                  = ui_state.use_ocio;
        runtime.blueprint.enabled        = ui_state.use_ocio;
        runtime.blueprint.wrapper_offset = ui_state.offset;

        error_message.clear();
        if (!ui_state.use_ocio)
            return true;

        OcioConfigSelection config_selection;
        resolve_ocio_config_selection(ui_state, config_selection);
        if (!load_ocio_config_from_selection(config_selection, runtime.config,
                                             error_message)) {
            return false;
        }

        runtime.blueprint.config_selection_key = config_selection.selection_key;
        runtime.blueprint.input_color_space
            = resolve_input_color_space(ui_state, image, runtime.config);
        runtime.blueprint.display = resolve_display_name(ui_state,
                                                         runtime.config);
        runtime.blueprint.view    = resolve_view_name(ui_state, runtime.config,
                                                      runtime.blueprint.display);
        if (runtime.blueprint.display.empty()
            || runtime.blueprint.view.empty()) {
            error_message = "Failed to resolve OCIO display/view";
            return false;
        }

        OCIO::ConstColorSpaceRcPtr scene_linear_space
            = runtime.config->getColorSpace("scene_linear");
        if (!scene_linear_space) {
            error_message = "Missing 'scene_linear' color space";
            return false;
        }

        OCIO::ColorSpaceTransformRcPtr input_transform
            = OCIO::ColorSpaceTransform::Create();
        input_transform->setSrc(runtime.blueprint.input_color_space.c_str());
        input_transform->setDst(scene_linear_space->getName());

        OCIO::ExposureContrastTransformRcPtr exposure_transform
            = OCIO::ExposureContrastTransform::Create();
        exposure_transform->makeExposureDynamic();

        OCIO::DisplayViewTransformRcPtr display_transform
            = OCIO::DisplayViewTransform::Create();
        display_transform->setSrc(scene_linear_space->getName());
        display_transform->setDisplay(runtime.blueprint.display.c_str());
        display_transform->setView(runtime.blueprint.view.c_str());

        OCIO::ExposureContrastTransformRcPtr gamma_transform
            = OCIO::ExposureContrastTransform::Create();
        gamma_transform->makeGammaDynamic();
        gamma_transform->setPivot(1.0);

        OCIO::GroupTransformRcPtr group_transform
            = OCIO::GroupTransform::Create();
        group_transform->appendTransform(input_transform);
        group_transform->appendTransform(exposure_transform);
        group_transform->appendTransform(display_transform);
        group_transform->appendTransform(gamma_transform);

        runtime.processor     = runtime.config->getProcessor(group_transform);
        runtime.gpu_processor = runtime.processor->getOptimizedGPUProcessor(
            OCIO::OPTIMIZATION_DEFAULT);

        runtime.shader_desc = OCIO::GpuShaderDesc::CreateShaderDesc();
        runtime.shader_desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_VK_4_6);
        runtime.shader_desc->setFunctionName(
            runtime.blueprint.function_name.c_str());
        runtime.shader_desc->setResourcePrefix(
            runtime.blueprint.resource_prefix.c_str());
        runtime.shader_desc->setAllowTexture1D(false);
        runtime.shader_desc->setDescriptorSetIndex(
            runtime.blueprint.descriptor_set_index,
            runtime.blueprint.texture_binding_start);
        runtime.gpu_processor->extractGpuShaderInfo(runtime.shader_desc);

        if (!populate_ocio_shader_blueprint(ui_state, runtime.processor,
                                            runtime.shader_desc,
                                            runtime.blueprint, error_message)) {
            runtime = {};
            return false;
        }

        if (runtime.shader_desc->hasDynamicProperty(
                OCIO::DYNAMIC_PROPERTY_GAMMA)) {
            OCIO::DynamicPropertyRcPtr prop
                = runtime.shader_desc->getDynamicProperty(
                    OCIO::DYNAMIC_PROPERTY_GAMMA);
            runtime.gamma_property = OCIO::DynamicPropertyValue::AsDouble(prop);
        }
        if (runtime.shader_desc->hasDynamicProperty(
                OCIO::DYNAMIC_PROPERTY_EXPOSURE)) {
            OCIO::DynamicPropertyRcPtr prop
                = runtime.shader_desc->getDynamicProperty(
                    OCIO::DYNAMIC_PROPERTY_EXPOSURE);
            runtime.exposure_property = OCIO::DynamicPropertyValue::AsDouble(
                prop);
        }
        return true;
    }

}  // namespace

void
reset_ocio_shader_blueprint(OcioShaderBlueprint& blueprint)
{
    blueprint = {};
}

const char*
ocio_config_source_name(OcioConfigSource source)
{
    switch (source) {
    case OcioConfigSource::Global: return "global";
    case OcioConfigSource::BuiltIn: return "builtin";
    case OcioConfigSource::User: return "user";
    default: return "global";
    }
}

void
resolve_ocio_config_selection(const PlaceholderUiState& ui_state,
                              OcioConfigSelection& selection)
{
    selection                  = {};
    selection.requested_source = ocio_config_source_from_int(
        ui_state.ocio_config_source);
    selection.env_value = std::string(
        OIIO::Strutil::strip(OIIO::Sysutil::getenv("OCIO")));
    const bool env_is_usable = ocio_source_string_is_usable(
        selection.env_value);

    const std::string user_path = std::string(
        OIIO::Strutil::strip(ui_state.ocio_user_config_path));
    std::error_code ec;
    const bool user_exists
        = !user_path.empty()
          && std::filesystem::exists(std::filesystem::path(user_path), ec)
          && !ec;

    switch (selection.requested_source) {
    case OcioConfigSource::Global:
        if (env_is_usable) {
            selection.resolved_source = OcioConfigSource::Global;
            selection.resolved_path   = selection.env_value;
        } else {
            selection.resolved_source  = OcioConfigSource::BuiltIn;
            selection.resolved_path    = builtin_ocio_config_path();
            selection.fallback_applied = true;
        }
        break;
    case OcioConfigSource::BuiltIn:
        selection.resolved_source = OcioConfigSource::BuiltIn;
        selection.resolved_path   = builtin_ocio_config_path();
        break;
    case OcioConfigSource::User:
        if (user_exists) {
            selection.resolved_source = OcioConfigSource::User;
            selection.resolved_path
                = std::filesystem::path(user_path).lexically_normal().string();
        } else {
            selection.resolved_source  = OcioConfigSource::BuiltIn;
            selection.resolved_path    = builtin_ocio_config_path();
            selection.fallback_applied = true;
        }
        break;
    default: selection.resolved_source = OcioConfigSource::Global; break;
    }

    selection.selection_key = OIIO::Strutil::fmt::format(
        "{}:{}", ocio_config_source_name(selection.resolved_source),
        selection.resolved_path);
}

void
destroy_ocio_shader_runtime(OcioShaderRuntime*& runtime)
{
    delete runtime;
    runtime = nullptr;
}

bool
build_ocio_shader_blueprint(const PlaceholderUiState& ui_state,
                            const LoadedImage* image,
                            OcioShaderBlueprint& blueprint,
                            std::string& error_message)
{
    OcioShaderRuntime runtime;
    try {
        if (!build_ocio_shader_runtime_internal(ui_state, image, runtime,
                                                error_message)) {
            reset_ocio_shader_blueprint(blueprint);
            blueprint.enabled = ui_state.use_ocio;
            return false;
        }
        blueprint = runtime.blueprint;
        return true;
    } catch (const OCIO::Exception& e) {
        error_message = e.what();
        reset_ocio_shader_blueprint(blueprint);
        blueprint.enabled = ui_state.use_ocio;
        return false;
    }
}

bool
ensure_ocio_shader_runtime(const PlaceholderUiState& ui_state,
                           const LoadedImage* image,
                           OcioShaderRuntime*& runtime,
                           std::string& error_message)
{
    error_message.clear();
    if (!ui_state.use_ocio) {
        destroy_ocio_shader_runtime(runtime);
        return true;
    }

    OcioConfigSelection config_selection;
    resolve_ocio_config_selection(ui_state, config_selection);
    OCIO::ConstConfigRcPtr config;
    if (!load_ocio_config_from_selection(config_selection, config,
                                         error_message)) {
        return false;
    }
    const std::string desired_input = resolve_input_color_space(ui_state, image,
                                                                config);
    if (runtime != nullptr
        && runtime->blueprint.config_selection_key
               == config_selection.selection_key
        && runtime->blueprint.input_color_space == desired_input
        && ((ui_state.ocio_display.empty() && runtime->blueprint.display.empty())
            || ui_state.ocio_display == "default"
            || runtime->blueprint.display == ui_state.ocio_display)
        && ((ui_state.ocio_view.empty() && runtime->blueprint.view.empty())
            || ui_state.ocio_view == "default"
            || runtime->blueprint.view == ui_state.ocio_view)) {
        runtime->blueprint.wrapper_offset = ui_state.offset;
        return true;
    }

    OcioShaderRuntime* rebuilt = new OcioShaderRuntime();
    try {
        if (!build_ocio_shader_runtime_internal(ui_state, image, *rebuilt,
                                                error_message)) {
            delete rebuilt;
            return false;
        }
    } catch (const OCIO::Exception& e) {
        error_message = e.what();
        delete rebuilt;
        return false;
    }

    destroy_ocio_shader_runtime(runtime);
    runtime = rebuilt;
    return true;
}

bool
build_ocio_uniform_buffer(OcioShaderRuntime& runtime,
                          const RendererPreviewControls& controls,
                          std::vector<unsigned char>& uniform_bytes,
                          std::string& error_message)
{
    error_message.clear();
    uniform_bytes.assign(runtime.blueprint.uniform_buffer_size, 0);
    if (runtime.shader_desc == nullptr)
        return true;

    if (runtime.exposure_property) {
        runtime.exposure_property->setValue(
            static_cast<double>(controls.exposure));
    }
    if (runtime.gamma_property) {
        const double gamma
            = 1.0 / std::max(1.0e-6, static_cast<double>(controls.gamma));
        runtime.gamma_property->setValue(gamma);
    }

    const unsigned num_uniforms = runtime.shader_desc->getNumUniforms();
    for (unsigned idx = 0; idx < num_uniforms; ++idx) {
        OCIO::GpuShaderDesc::UniformData data;
        runtime.shader_desc->getUniform(idx, data);
        switch (data.m_type) {
        case OCIO::UNIFORM_DOUBLE: {
            const float value = data.m_getDouble
                                    ? static_cast<float>(data.m_getDouble())
                                    : 0.0f;
            if (!write_uniform_bytes(uniform_bytes, data.m_bufferOffset, &value,
                                     sizeof(value), error_message)) {
                return false;
            }
            break;
        }
        case OCIO::UNIFORM_BOOL: {
            const int32_t value = data.m_getBool && data.m_getBool() ? 1 : 0;
            if (!write_uniform_bytes(uniform_bytes, data.m_bufferOffset, &value,
                                     sizeof(value), error_message)) {
                return false;
            }
            break;
        }
        case OCIO::UNIFORM_FLOAT3: {
            float value[3] = { 0.0f, 0.0f, 0.0f };
            if (data.m_getFloat3) {
                const OCIO::Float3& src = data.m_getFloat3();
                value[0]                = src[0];
                value[1]                = src[1];
                value[2]                = src[2];
            }
            if (!write_uniform_bytes(uniform_bytes, data.m_bufferOffset, value,
                                     sizeof(value), error_message)) {
                return false;
            }
            break;
        }
        case OCIO::UNIFORM_VECTOR_FLOAT: {
            const int count  = data.m_vectorFloat.m_getSize
                                   ? data.m_vectorFloat.m_getSize()
                                   : 0;
            const float* src = data.m_vectorFloat.m_getVector
                                   ? data.m_vectorFloat.m_getVector()
                                   : nullptr;
            for (int i = 0; i < count; ++i) {
                const size_t elem_offset = data.m_bufferOffset
                                           + static_cast<size_t>(i) * 16u;
                const float value = src ? src[i] : 0.0f;
                if (!write_uniform_bytes(uniform_bytes, elem_offset, &value,
                                         sizeof(value), error_message)) {
                    return false;
                }
            }
            break;
        }
        case OCIO::UNIFORM_VECTOR_INT: {
            const int count = data.m_vectorInt.m_getSize
                                  ? data.m_vectorInt.m_getSize()
                                  : 0;
            const int* src  = data.m_vectorInt.m_getVector
                                  ? data.m_vectorInt.m_getVector()
                                  : nullptr;
            for (int i = 0; i < count; ++i) {
                const size_t elem_offset = data.m_bufferOffset
                                           + static_cast<size_t>(i) * 16u;
                const int32_t value = src ? src[i] : 0;
                if (!write_uniform_bytes(uniform_bytes, elem_offset, &value,
                                         sizeof(value), error_message)) {
                    return false;
                }
            }
            break;
        }
        case OCIO::UNIFORM_UNKNOWN:
        default: break;
        }
    }
    return true;
}

bool
query_ocio_menu_data(const PlaceholderUiState& ui_state,
                     std::vector<std::string>& image_color_spaces,
                     std::vector<std::string>& displays,
                     std::vector<std::string>& views,
                     std::string& resolved_display, std::string& resolved_view,
                     std::string& error_message)
{
    image_color_spaces.clear();
    displays.clear();
    views.clear();
    resolved_display.clear();
    resolved_view.clear();
    error_message.clear();

    try {
        OcioConfigSelection config_selection;
        resolve_ocio_config_selection(ui_state, config_selection);
        OCIO::ConstConfigRcPtr config;
        if (!load_ocio_config_from_selection(config_selection, config,
                                             error_message)) {
            return false;
        }

        const int num_color_spaces = config->getNumColorSpaces();
        image_color_spaces.reserve(static_cast<size_t>(num_color_spaces) + 1u);
        image_color_spaces.emplace_back("auto");
        for (int i = 0; i < num_color_spaces; ++i) {
            const char* color_space = config->getColorSpaceNameByIndex(i);
            if (color_space != nullptr && color_space[0] != '\0')
                image_color_spaces.emplace_back(color_space);
        }

        const int num_displays = config->getNumDisplays();
        displays.reserve(static_cast<size_t>(num_displays));
        for (int i = 0; i < num_displays; ++i) {
            const char* display_name = config->getDisplay(i);
            if (display_name != nullptr && display_name[0] != '\0')
                displays.emplace_back(display_name);
        }

        resolved_display = resolve_display_name(ui_state, config);
        resolved_view = resolve_view_name(ui_state, config, resolved_display);
        if (!resolved_display.empty()) {
            const int num_views = config->getNumViews(resolved_display.c_str());
            views.reserve(static_cast<size_t>(num_views));
            for (int i = 0; i < num_views; ++i) {
                const char* view_name
                    = config->getView(resolved_display.c_str(), i);
                if (view_name != nullptr && view_name[0] != '\0')
                    views.emplace_back(view_name);
            }
        }
        return true;
    } catch (const OCIO::Exception& e) {
        error_message = e.what();
        return false;
    }
}

bool
build_ocio_preview_fragment_source(const OcioShaderBlueprint& blueprint,
                                   std::string& shader_source,
                                   std::string& error_message)
{
    shader_source.clear();
    error_message.clear();
    if (!blueprint.enabled) {
        error_message = "OCIO preview source requested while OCIO is disabled";
        return false;
    }
    if (!blueprint.valid) {
        error_message
            = "OCIO preview source requested without a valid blueprint";
        return false;
    }
    if (blueprint.shader_text.empty()) {
        error_message = "OCIO shader text is empty";
        return false;
    }

    shader_source = OIIO::Strutil::fmt::format(
        R"glsl(#version 460 core
layout(location = 0) in vec2 uv_in;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D source_image;

layout(push_constant) uniform PreviewPushConstants {{
    float exposure;
    float gamma;
    float offset;
    int color_mode;
    int channel;
    int use_ocio;
    int orientation;
}} pc;

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
    vec2 src_uv = display_to_source_uv(uv_in, pc.orientation);
    vec4 c = texture(source_image, src_uv);
    c.rgb += vec3(pc.offset);

    if (pc.color_mode == 1) {{
        c.a = 1.0;
    }} else if (pc.color_mode == 2) {{
        float v = selected_channel(c, pc.channel);
        c = vec4(v, v, v, 1.0);
    }} else if (pc.color_mode == 3) {{
        float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
        c = vec4(y, y, y, 1.0);
    }} else if (pc.color_mode == 4) {{
        float v = selected_channel(c, pc.channel);
        c = vec4(heatmap(v), 1.0);
    }}

    if (pc.channel > 0 && pc.color_mode != 2 && pc.color_mode != 4) {{
        float v = selected_channel(c, pc.channel);
        c = vec4(v, v, v, 1.0);
    }}

    c = {}(c);
    out_color = c;
}}
)glsl",
        blueprint.shader_text, blueprint.function_name);
    return true;
}

bool
compile_ocio_preview_fragment_spirv(const OcioShaderBlueprint& blueprint,
                                    std::vector<uint32_t>& spirv_words,
                                    std::string& error_message)
{
    std::string shader_source;
    if (!build_ocio_preview_fragment_source(blueprint, shader_source,
                                            error_message)) {
        spirv_words.clear();
        return false;
    }
    return compile_glsl_to_spirv(RuntimeShaderStage::Fragment, shader_source,
                                 "imiv.ocio.preview.frag", spirv_words,
                                 error_message);
}

bool
preflight_ocio_runtime_shader(const PlaceholderUiState& ui_state,
                              const LoadedImage* image,
                              std::string& error_message)
{
    OcioShaderBlueprint blueprint;
    std::vector<uint32_t> spirv_words;
    if (!build_ocio_shader_blueprint(ui_state, image, blueprint, error_message))
        return false;
    if (!blueprint.enabled)
        return true;
    return compile_ocio_preview_fragment_spirv(blueprint, spirv_words,
                                               error_message);
}

}  // namespace Imiv
