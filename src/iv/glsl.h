// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_GLSL_H
#define INCLUDED_OCIO_GLSL_H

#include <vector>

#include <OpenColorIO/OpenColorIO.h>


namespace OCIO_NAMESPACE
{

class OpenGLBuilder;
typedef OCIO_SHARED_PTR<OpenGLBuilder> OpenGLBuilderRcPtr;


// This is a reference implementation showing how to do the texture upload & allocation,
// and the program compilation for the GLSL shader language.

class OpenGLBuilder
{
    struct TextureId
    {
        unsigned    m_uid = -1;
        std::string m_textureName;
        std::string m_samplerName;
        unsigned    m_type = -1;

        TextureId(unsigned uid,
                  const std::string & textureName,
                  const std::string & samplerName,
                  unsigned type)
            :   m_uid(uid)
            ,   m_textureName(textureName)
            ,   m_samplerName(samplerName)
            ,   m_type(type)
        {}
    };

    typedef std::vector<TextureId> TextureIds;

    // Uniform are used for dynamic parameters.
    class Uniform
    {
    public:
        Uniform(const std::string & name, const GpuShaderDesc::UniformData & data);

        void setUp(unsigned program);

        void use();

    private:
        Uniform() = delete;
        std::string m_name;
        GpuShaderDesc::UniformData m_data;

        unsigned m_handle;
    };
    typedef std::vector<Uniform> Uniforms;

public:
    // Create an OpenGL builder using the GPU shader information from a specific processor
    static OpenGLBuilderRcPtr Create(const GpuShaderDescRcPtr & gpuShader);

    ~OpenGLBuilder();

    inline void setVerbose(bool verbose) { m_verbose = verbose; }
    inline bool isVerbose() const { return m_verbose; }

    // Allocate & upload all the needed textures
    //  (i.e. the index is the first available index for any kind of textures).
    void allocateAllTextures(unsigned startIndex);
    void useAllTextures();

    // Update all uniforms.
    void useAllUniforms();

    // Build the complete shader program which includes the OCIO shader program 
    // and the client shader program.
    unsigned buildProgram(const std::string & clientShaderProgram, bool standaloneShader);
    void useProgram();
    unsigned getProgramHandle();

    // Determine the maximum width value of a texture
    // depending of the graphic card and its driver.
    static unsigned GetTextureMaxWidth();

protected:
    OpenGLBuilder(const GpuShaderDescRcPtr & gpuShader);

    // Prepare all the needed uniforms.
    void linkAllUniforms();

    void deleteAllTextures();
    void deleteAllUniforms();

    // To add the version to the fragment shader program (so that GLSL does not use the default
    // of 1.10 when the minimum version for OCIO is 1.20).
    std::string getGLSLVersionString();

private:
    OpenGLBuilder();
    OpenGLBuilder(const OpenGLBuilder &) = delete;
    OpenGLBuilder& operator=(const OpenGLBuilder &) = delete;

    const GpuShaderDescRcPtr m_shaderDesc; // Description of the fragment shader to create
    unsigned m_startIndex;                 // Starting index for texture allocations
    TextureIds m_textureIds;               // Texture ids of all needed textures
    Uniforms m_uniforms;                   // Vector of dynamic parameters
    unsigned m_fragShader;                 // Fragment shader identifier
    unsigned m_program;                    // Program identifier
    std::string m_shaderCacheID;           // Current shader program key
    bool m_verbose;                        // Print shader code to std::cout for debugging purposes
};

} // namespace OCIO_NAMESPACE

#endif // INCLUDED_OCIO_GLSL_H

