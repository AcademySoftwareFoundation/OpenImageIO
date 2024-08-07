// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#ifndef OPENIMAGEIO_IVGL_OCIO_H
#define OPENIMAGEIO_IVGL_OCIO_H

#include <OpenColorIO/OpenColorIO.h>

#define MAKE_OCIO_VERSION_HEX(maj, min, patch) \
    (((maj) << 24) | ((min) << 16) | (patch))

#include "ivgl.h"

#include <vector>

using namespace OIIO;

namespace OCIO = OCIO_NAMESPACE;
using namespace OCIO;

class IvGL_OCIO : public IvGL {
public:
    IvGL_OCIO(QWidget* parent, ImageViewer& viewer);
    virtual ~IvGL_OCIO();

protected:
    void update_state(void) override;
    void use_program(void) override;
    const char* color_func_shader_text() override;
    void update_uniforms(int tex_width, int tex_height,
                         bool pixelview) override;

private:
    struct TextureDesc {
        unsigned m_uid = -1;
        std::string m_textureName;
        std::string m_samplerName;
        unsigned m_type = -1;

        TextureDesc(unsigned uid, const std::string& textureName,
                    const std::string& samplerName, unsigned type)
            : m_uid(uid)
            , m_textureName(textureName)
            , m_samplerName(samplerName)
            , m_type(type)
        {
        }
    };

    struct UniformDesc {
        std::string m_name;
        GpuShaderDesc::UniformData m_data;
        unsigned m_handle;

        UniformDesc(const std::string& name,
                    const GpuShaderDesc::UniformData& data)
            : m_name(name)
            , m_data(data)
            , m_handle(0)
        {
        }
    };

    std::vector<TextureDesc> m_textures;
    std::vector<UniformDesc> m_uniforms;

    bool m_current_use_ocio = false;
    std::string m_current_color_space;
    std::string m_current_display;
    std::string m_current_view;
    std::string m_current_look;

    OCIO::GpuShaderDescRcPtr m_shader_desc;
    OCIO::DynamicPropertyDoubleRcPtr m_gamma_property;
    OCIO::DynamicPropertyDoubleRcPtr m_exposure_property;

    unsigned m_start_index;  // Starting index for texture allocations

    void allocate_texture_3D(unsigned index, unsigned& texId,
                             Interpolation interpolation, unsigned edgelen,
                             const float* values);

    void allocate_texture_2D(unsigned index, unsigned& texId, unsigned width,
                             unsigned height,
                             GpuShaderDesc::TextureType channel,
                             Interpolation interpolation, const float* values);

    void on_ocio_error(const char* message);
    void reset(void);
    void allocate_all_textures(unsigned start_index);
    void use_all_textures();
    void set_texture_parameters(GLenum texture_type,
                                Interpolation interpolation);
};

#endif  // OPENIMAGEIO_IVGL_OCIO_H
