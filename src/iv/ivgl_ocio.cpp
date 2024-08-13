// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "ivgl_ocio.h"
#include "imageviewer.h"

#include <QLabel>


IvGL_OCIO::IvGL_OCIO(QWidget* parent, ImageViewer& viewer)
    : IvGL(parent, viewer)
{
}

IvGL_OCIO::~IvGL_OCIO() {}

void
IvGL_OCIO::update_state()
{
    if (!m_viewer.useOCIO()) {
        if (m_current_use_ocio) {
            m_color_shader_text = "";
            m_current_use_ocio  = false;
        }
        IvGL::update_state();
        return;
    }

    IvImage* img = m_viewer.cur();
    if (!img)
        return;

    bool update_shader = false;

    if (!m_current_use_ocio) {
        m_current_use_ocio = true;
        update_shader      = true;
    }

    const char* ocio_color_space = m_viewer.ocioColorSpace().c_str();
    if (m_current_color_space != ocio_color_space) {
        m_current_color_space = ocio_color_space;
        update_shader         = true;
    }

    const char* ocio_display = m_viewer.ocioDisplay().c_str();
    if (m_current_display != ocio_display) {
        m_current_display = ocio_display;
        update_shader     = true;
    }

    const char* ocio_view = m_viewer.ocioView().c_str();
    if (m_current_view != ocio_view) {
        m_current_view = ocio_view;
        update_shader  = true;
    }

    if (update_shader) {
        try {
            OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

            OCIO::ConstColorSpaceRcPtr scene_linear_space
                = config->getColorSpace("scene_linear");

            if (!scene_linear_space) {
                on_ocio_error("Missing 'scene_linear' color space");
                return;
            }

            OCIO::ColorSpaceTransformRcPtr input_transform
                = OCIO::ColorSpaceTransform::Create();
            input_transform->setSrc(ocio_color_space);
            input_transform->setDst(scene_linear_space->getName());

            OCIO::ExposureContrastTransformRcPtr exposure_transform
                = OCIO::ExposureContrastTransform::Create();
            exposure_transform->makeExposureDynamic();

            OCIO::DisplayViewTransformRcPtr display_transform
                = OCIO::DisplayViewTransform::Create();
            display_transform->setSrc(scene_linear_space->getName());
            display_transform->setDisplay(ocio_display);
            display_transform->setView(ocio_view);

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

            OCIO::ConstProcessorRcPtr processor = config->getProcessor(
                group_transform);

            if (m_shader_desc) {
                reset();
            }

            OCIO::GpuShaderDescRcPtr shaderDesc
                = OCIO::GpuShaderDesc::CreateShaderDesc();
            shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_2);
            shaderDesc->setFunctionName("ColorFunc");
            shaderDesc->setResourcePrefix("ocio_");

            OCIO::ConstGPUProcessorRcPtr gpuProcessor
                = processor->getOptimizedGPUProcessor(
                    OCIO::OPTIMIZATION_DEFAULT);
            gpuProcessor->extractGpuShaderInfo(shaderDesc);

            m_shader_desc = shaderDesc;

            allocate_all_textures(m_texbufs.size() + 1);

            create_shaders();

            m_uniforms.clear();

            const unsigned maxUniforms = shaderDesc->getNumUniforms();
            for (unsigned idx = 0; idx < maxUniforms; ++idx) {
                GpuShaderDesc::UniformData data;
                const char* name = shaderDesc->getUniform(idx, data);
                if (data.m_type == UNIFORM_UNKNOWN) {
                    throw Exception("Unknown uniform type.");
                }
                // Transfer uniform.
                UniformDesc uniform(name, data);
                uniform.m_handle = glGetUniformLocation(m_shader_program, name);
                m_uniforms.push_back(uniform);

                std::string error;
                if (glGetError()) {
                    std::string err("Shader parameter ");
                    err += name;
                    err += " not found: ";
                    throw Exception(err.c_str());
                }
            }

            OCIO::DynamicPropertyRcPtr prop1 = shaderDesc->getDynamicProperty(
                OCIO::DYNAMIC_PROPERTY_GAMMA);
            m_gamma_property = OCIO::DynamicPropertyValue::AsDouble(prop1);

            OCIO::DynamicPropertyRcPtr prop2 = shaderDesc->getDynamicProperty(
                OCIO::DYNAMIC_PROPERTY_EXPOSURE);
            m_exposure_property = OCIO::DynamicPropertyValue::AsDouble(prop2);
        } catch (const OCIO::Exception& e) {
            on_ocio_error(e.what());
            return;
        }
    }
}

void
IvGL_OCIO::use_program(void)
{
    if (m_viewer.useOCIO() && m_shader_desc) {
        IvImage* img = m_viewer.cur();
        if (!img)
            return;

        glUseProgram(m_shader_program);
        print_error("OCIO After use program");

        use_all_textures();
        print_error("OCIO After use textures");

        double gamma = 1.0 / std::max(1e-6, static_cast<double>(img->gamma()));
        m_gamma_property->setValue(gamma);
        m_exposure_property->setValue(img->exposure());
    } else {
        IvGL::use_program();
    }
}

const char*
IvGL_OCIO::color_func_shader_text()
{
    if (m_viewer.useOCIO() && m_shader_desc) {
        return m_shader_desc->getShaderText();
    } else {
        return IvGL::color_func_shader_text();
    }
}

void
IvGL_OCIO::on_ocio_error(const char* message)
{
    m_viewer.statusImgInfo->setText(tr("OCIO error: %1.").arg(message));

    if (m_shader_desc != nullptr) {
        reset();
    }
}

void
IvGL_OCIO::update_uniforms(int tex_width, int tex_height, bool pixelview)
{
    IvGL::update_uniforms(tex_width, tex_height, pixelview);

    if (!m_viewer.useOCIO() || !m_shader_desc) {
        return;
    }

    for (auto i = m_uniforms.begin(); i != m_uniforms.end(); ++i) {
        const UniformDesc& uniform             = *i;
        const GpuShaderDesc::UniformData& data = uniform.m_data;
        const unsigned handle                  = uniform.m_handle;

        // Update value.
        if (data.m_getDouble) {
            glUniform1f(handle, (GLfloat)data.m_getDouble());
        } else if (data.m_getBool) {
            glUniform1f(handle, (GLfloat)(data.m_getBool() ? 1.0f : 0.0f));
        } else if (data.m_getFloat3) {
            glUniform3f(handle, (GLfloat)data.m_getFloat3()[0],
                        (GLfloat)data.m_getFloat3()[1],
                        (GLfloat)data.m_getFloat3()[2]);
        } else if (data.m_vectorFloat.m_getSize
                   && data.m_vectorFloat.m_getVector) {
            glUniform1fv(handle, (GLsizei)data.m_vectorFloat.m_getSize(),
                         (GLfloat*)data.m_vectorFloat.m_getVector());
        } else if (data.m_vectorInt.m_getSize && data.m_vectorInt.m_getVector) {
            glUniform1iv(handle, (GLsizei)data.m_vectorInt.m_getSize(),
                         (GLint*)data.m_vectorInt.m_getVector());
        } else {
            throw Exception("Uniform is not linked to any value.");
        }
    }
}

void
IvGL_OCIO::reset(void)
{
    m_shader_desc.reset();


    for (auto i = m_textures.begin(); i != m_textures.end(); ++i)
        glDeleteTextures(1, &(i->m_uid));

    m_textures.clear();
}

void
IvGL_OCIO::allocate_texture_3D(unsigned index, unsigned& texId,
                               Interpolation interpolation, unsigned edgelen,
                               const float* values)
{
    if (values == nullptr) {
        throw Exception("Missing texture data");
    }

    glGenTextures(1, &texId);
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_3D, texId);
    set_texture_parameters(GL_TEXTURE_3D, interpolation);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F_ARB, edgelen, edgelen, edgelen, 0,
                 GL_RGB, GL_FLOAT, values);
}

void
IvGL_OCIO::allocate_texture_2D(unsigned index, unsigned& texId, unsigned width,
                               unsigned height,
                               GpuShaderDesc::TextureType channel,
                               Interpolation interpolation, const float* values)
{
    if (values == nullptr) {
        throw Exception("Missing texture data.");
    }

    GLint internalformat = GL_RGB32F_ARB;
    GLenum format        = GL_RGB;

    if (channel == GpuShaderCreator::TEXTURE_RED_CHANNEL) {
        internalformat = GL_R32F;
        format         = GL_RED;
    }

    glGenTextures(1, &texId);
    glActiveTexture(GL_TEXTURE0 + index);

    if (height > 1) {
        glBindTexture(GL_TEXTURE_2D, texId);
        set_texture_parameters(GL_TEXTURE_2D, interpolation);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format,
                     GL_FLOAT, values);
    } else {
        glBindTexture(GL_TEXTURE_1D, texId);
        set_texture_parameters(GL_TEXTURE_1D, interpolation);
        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format,
                     GL_FLOAT, values);
    }
}

void
IvGL_OCIO::set_texture_parameters(GLenum texture_type,
                                  Interpolation interpolation)
{
    if (interpolation == INTERP_NEAREST) {
        glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(texture_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(texture_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void
IvGL_OCIO::allocate_all_textures(unsigned start_index)
{
    // This is the first available index for the textures.
    m_start_index      = start_index;
    unsigned currIndex = m_start_index;

    // Process the 3D LUT first.

    const unsigned maxTexture3D = m_shader_desc->getNum3DTextures();
    for (unsigned idx = 0; idx < maxTexture3D; ++idx) {
        const char* textureName     = nullptr;
        const char* samplerName     = nullptr;
        unsigned edgelen            = 0;
        Interpolation interpolation = INTERP_LINEAR;
        m_shader_desc->get3DTexture(idx, textureName, samplerName, edgelen,
                                    interpolation);

        if (!textureName || !*textureName || !samplerName || !*samplerName
            || edgelen == 0) {
            throw Exception("The texture data is corrupted");
        }

        const float* values = nullptr;
        m_shader_desc->get3DTextureValues(idx, values);
        if (!values) {
            throw Exception("The texture values are missing");
        }

        unsigned texId = 0;
        allocate_texture_3D(currIndex, texId, interpolation, edgelen, values);
        m_textures.emplace_back(texId, textureName, samplerName, GL_TEXTURE_3D);
        currIndex++;
    }

    // Process the 1D LUTs.

    const unsigned maxTexture2D = m_shader_desc->getNumTextures();
    for (unsigned idx = 0; idx < maxTexture2D; ++idx) {
        const char* textureName            = nullptr;
        const char* samplerName            = nullptr;
        unsigned width                     = 0;
        unsigned height                    = 0;
        GpuShaderDesc::TextureType channel = GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        Interpolation interpolation        = INTERP_LINEAR;
#if OCIO_VERSION_HEX >= MAKE_OCIO_VERSION_HEX(2, 3, 0)
        GpuShaderCreator::TextureDimensions dimensions
            = GpuShaderCreator::TextureDimensions::TEXTURE_2D;
        m_shader_desc->getTexture(idx, textureName, samplerName, width, height,
                                  channel, dimensions, interpolation);
#else
        m_shader_desc->getTexture(idx, textureName, samplerName, width, height,
                                  channel, interpolation);
#endif

        if (!textureName || !*textureName || !samplerName || !*samplerName
            || width == 0) {
            throw Exception("The texture data is corrupted");
        }

        const float* values = 0x0;
        m_shader_desc->getTextureValues(idx, values);
        if (!values) {
            throw Exception("The texture values are missing");
        }

        unsigned texId = 0;
        allocate_texture_2D(currIndex, texId, width, height, channel,
                            interpolation, values);

        unsigned type = (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D;
        m_textures.emplace_back(texId, textureName, samplerName, type);
        currIndex++;
    }
}

void
IvGL_OCIO::use_all_textures()
{
    const size_t size = m_textures.size();

    for (size_t i = 0; i < size; ++i) {
        const TextureDesc& data = m_textures[i];

        glActiveTexture((GLenum)(GL_TEXTURE0 + m_start_index + i));
        glBindTexture(data.m_type, data.m_uid);
        glUniform1i(glGetUniformLocation(m_shader_program,
                                         data.m_samplerName.c_str()),
                    GLint(m_start_index + i));
    }
}


//OIIO_PRAGMA_WARNING_POP
