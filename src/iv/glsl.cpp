// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

// clang-format off

//#ifdef __APPLE__
//
//#include <OpenGL/gl.h>
//#include <OpenGL/glext.h>
//
//#elif _WIN32
//
//#include <GL/glew.h>
//
//#else
//
//#include <GL/glew.h>
//#include <GL/gl.h>
//
//#endif

#include <sstream>
#include <iostream>

#include <OpenColorIO/OpenColorIO.h>

#include "glsl.h"

using namespace OCIO_NAMESPACE;

namespace OIIO_OCIO
{

bool OpenGLBuilder::GetGLError(std::string & error)
{
    const GLenum glErr = glGetError();
    if(glErr!=GL_NO_ERROR)
    {
//#ifdef __APPLE__
        // Unfortunately no gluErrorString equivalent on Mac.
        error = "OpenGL Error";
//#else
//        error = (const char*)gluErrorString(glErr);
//#endif
        return true;
    }
    return false;
}

void OpenGLBuilder::CheckStatus()
{
    std::string error;
    if (GetGLError(error))
    {
        throw Exception(error.c_str());
    }
}

void OpenGLBuilder::SetTextureParameters(GLenum textureType, Interpolation interpolation)
{
    if(interpolation==INTERP_NEAREST)
    {
        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void OpenGLBuilder::AllocateTexture3D(unsigned index, unsigned & texId,
                        Interpolation interpolation,
                        unsigned edgelen, const float * values)
{
    if(values==0x0)
    {
        throw Exception("Missing texture data");
    }

    glGenTextures(1, &texId);

    glActiveTexture(GL_TEXTURE0 + index);

    glBindTexture(GL_TEXTURE_3D, texId);

    SetTextureParameters(GL_TEXTURE_3D, interpolation);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F_ARB,
                    edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
}

void OpenGLBuilder::AllocateTexture2D(unsigned index, unsigned & texId,
                       unsigned width, unsigned height,
                       GpuShaderDesc::TextureType channel,
                       Interpolation interpolation, const float * values)
{
    if (values == nullptr)
    {
        throw Exception("Missing texture data.");
    }

    GLint internalformat = GL_RGB32F_ARB;
    GLenum format        = GL_RGB;

    if (channel == GpuShaderCreator::TEXTURE_RED_CHANNEL)
    {
        internalformat = GL_R32F;
        format         = GL_RED;
    }

    glGenTextures(1, &texId);

    glActiveTexture(GL_TEXTURE0 + index);

    if (height > 1)
    {
        glBindTexture(GL_TEXTURE_2D, texId);

        SetTextureParameters(GL_TEXTURE_2D, interpolation);

        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, values);
    }
    else
    {
        glBindTexture(GL_TEXTURE_1D, texId);

        SetTextureParameters(GL_TEXTURE_1D, interpolation);

        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
    }
}

GLuint OpenGLBuilder::CompileShaderText(GLenum shaderType, const char * text)
{
    CheckStatus();

    if(!text || !*text)
    {
        throw Exception("Invalid fragment shader program");
    }

    GLuint shader;
    GLint stat;

    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar **) &text, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &stat);

    if (!stat)
    {
        GLchar log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);

        std::string err("OCIO Shader program compilation failed: ");
        err += log;
        err += "\n";
        err += text;

        throw Exception(err.c_str());
    }

    return shader;
}

void OpenGLBuilder::LinkShaders(GLuint program, GLuint fragShader)
{
    CheckStatus();

    if (!fragShader)
    {
        throw Exception("Missing shader program");
    }
    else
    {
        glAttachShader(program, fragShader);
    }

    glLinkProgram(program);

    GLint stat;
    glGetProgramiv(program, GL_LINK_STATUS, &stat);
    if (!stat)
    {
        GLchar log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, log);

        std::string err("Shader link error:\n");
        err += log;
        throw Exception(err.c_str());
    }
}

OpenGLBuilder::Uniform::Uniform(const std::string & name, const GpuShaderDesc::UniformData & data, QOpenGLContext *context)
    : QOpenGLFunctions(context)
    , m_name(name)
    , m_data(data)
    , m_handle(0)
{
}

void OpenGLBuilder::Uniform::setUp(unsigned program)
{
    m_handle = glGetUniformLocation(program, m_name.c_str());

//    std::string error;
//    if (GetGLError(error))
//    {
//        std::string err("Shader parameter ");
//        err += m_name;
//        err += " not found: ";
//        throw Exception(err.c_str());
//    }
}

void OpenGLBuilder::Uniform::use()
{
    // Update value.
    if (m_data.m_getDouble)
    {
        glUniform1f(m_handle, (GLfloat)m_data.m_getDouble());
    }
    else if (m_data.m_getBool)
    {
        glUniform1f(m_handle, (GLfloat)(m_data.m_getBool()?1.0f:0.0f));
    }
    else if (m_data.m_getFloat3)
    {
        glUniform3f(m_handle, (GLfloat)m_data.m_getFloat3()[0],
                              (GLfloat)m_data.m_getFloat3()[1],
                              (GLfloat)m_data.m_getFloat3()[2]);
    }
    else if (m_data.m_vectorFloat.m_getSize && m_data.m_vectorFloat.m_getVector)
    {
        glUniform1fv(m_handle, (GLsizei)m_data.m_vectorFloat.m_getSize(),
                               (GLfloat*)m_data.m_vectorFloat.m_getVector());
    }
    else if (m_data.m_vectorInt.m_getSize && m_data.m_vectorInt.m_getVector)
    {
        glUniform1iv(m_handle, (GLsizei)m_data.m_vectorInt.m_getSize(),
                               (GLint*)m_data.m_vectorInt.m_getVector());
    }
    else
    {
        throw Exception("Uniform is not linked to any value.");
    }
}


//////////////////////////////////////////////////////////

OpenGLBuilderRcPtr OpenGLBuilder::Create(const GpuShaderDescRcPtr & shaderDesc, QOpenGLContext *context)
{
    return OpenGLBuilderRcPtr(new OpenGLBuilder(shaderDesc, context));
}

OpenGLBuilder::OpenGLBuilder(const GpuShaderDescRcPtr & shaderDesc, QOpenGLContext *context)
    :   QOpenGLFunctions(context)
    ,   m_shaderDesc(shaderDesc)
    ,   m_startIndex(0)
    ,   m_fragShader(0)
    ,   m_program(glCreateProgram())
    ,   m_verbose(false)
{
}

OpenGLBuilder::~OpenGLBuilder()
{
    deleteAllTextures();

    if(m_fragShader)
    {
        glDetachShader(m_program, m_fragShader);
        glDeleteShader(m_fragShader);
        m_fragShader = 0;
    }

    if(m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void OpenGLBuilder::allocateAllTextures(unsigned startIndex)
{
    deleteAllTextures();

    // This is the first available index for the textures.
    m_startIndex = startIndex;
    unsigned currIndex = m_startIndex;

    // Process the 3D LUT first.

    const unsigned maxTexture3D = m_shaderDesc->getNum3DTextures();
    for(unsigned idx=0; idx<maxTexture3D; ++idx)
    {
        // 1. Get the information of the 3D LUT.

        const char * textureName = nullptr;
        const char * samplerName = nullptr;
        unsigned edgelen = 0;
        Interpolation interpolation = INTERP_LINEAR;
        m_shaderDesc->get3DTexture(idx, textureName, samplerName, edgelen, interpolation);

        if(!textureName || !*textureName
            || !samplerName || !*samplerName
            || edgelen==0)
        {
            throw Exception("The texture data is corrupted");
        }

        const float * values = nullptr;
        m_shaderDesc->get3DTextureValues(idx, values);
        if(!values)
        {
            throw Exception("The texture values are missing");
        }

        // 2. Allocate the 3D LUT.

        unsigned texId = 0;
        AllocateTexture3D(currIndex, texId, interpolation, edgelen, values);

        // 3. Keep the texture id & name for the later enabling.

        m_textureIds.push_back(TextureId(texId, textureName, samplerName, GL_TEXTURE_3D));

        currIndex++;
    }

    // Process the 1D LUTs.

    const unsigned maxTexture2D = m_shaderDesc->getNumTextures();
    for(unsigned idx=0; idx<maxTexture2D; ++idx)
    {
        // 1. Get the information of the 1D LUT.

        const char * textureName = nullptr;
        const char * samplerName = nullptr;
        unsigned width = 0;
        unsigned height = 0;
        GpuShaderDesc::TextureType channel = GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        Interpolation interpolation = INTERP_LINEAR;
        m_shaderDesc->getTexture(idx, textureName, samplerName, width, height, channel, interpolation);

        if (!textureName || !*textureName
            || !samplerName || !*samplerName
            || width==0)
        {
            throw Exception("The texture data is corrupted");
        }

        const float * values = 0x0;
        m_shaderDesc->getTextureValues(idx, values);
        if(!values)
        {
            throw Exception("The texture values are missing");
        }

        // 2. Allocate the 1D LUT (a 2D texture is needed to hold large LUTs).

        unsigned texId = 0;
        AllocateTexture2D(currIndex, texId, width, height, channel, interpolation, values);

        // 3. Keep the texture id & name for the later enabling.

        unsigned type = (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D;
        m_textureIds.push_back(TextureId(texId, textureName, samplerName, type));
        currIndex++;
    }
}

void OpenGLBuilder::deleteAllTextures()
{
    const size_t max = m_textureIds.size();
    for (size_t idx=0; idx<max; ++idx)
    {
        const TextureId & data = m_textureIds[idx];
        glDeleteTextures(1, &data.m_uid);
    }

    m_textureIds.clear();
}

void OpenGLBuilder::useAllTextures()
{
    const size_t max = m_textureIds.size();
    for (size_t idx=0; idx<max; ++idx)
    {
        const TextureId& data = m_textureIds[idx];
        glActiveTexture((GLenum)(GL_TEXTURE0 + m_startIndex + idx));
        glBindTexture(data.m_type, data.m_uid);
        glUniform1i(
            glGetUniformLocation(m_program,
                                 data.m_samplerName.c_str()),
                                 GLint(m_startIndex + idx) );
    }
}

void OpenGLBuilder::linkAllUniforms(QOpenGLContext *context)
{
    deleteAllUniforms();

    const unsigned maxUniforms = m_shaderDesc->getNumUniforms();
    for (unsigned idx = 0; idx < maxUniforms; ++idx)
    {
        GpuShaderDesc::UniformData data;
        const char * name = m_shaderDesc->getUniform(idx, data);
        if (data.m_type == UNIFORM_UNKNOWN)
        {
            throw Exception("Unknown uniform type.");
        }
        // Transfer uniform.
        m_uniforms.emplace_back(name, data, context);
        // Connect uniform with program.
        m_uniforms.back().setUp(m_program);
        
        std::string error;
        if (GetGLError(error))
        {
            std::string err("Shader parameter ");
            err += name;
            err += " not found: ";
            throw Exception(err.c_str());
        }
    }
}

void OpenGLBuilder::deleteAllUniforms()
{
    m_uniforms.clear();
}

void OpenGLBuilder::useAllUniforms()
{
    for (auto uniform : m_uniforms)
    {
        uniform.use();
    }
}

std::string OpenGLBuilder::getGLSLVersionString()
{
    switch (m_shaderDesc->getLanguage())
    {
    case GPU_LANGUAGE_GLSL_1_2:
    case GPU_LANGUAGE_MSL_2_0:
        // That's the minimal version supported.
        return "#version 120";
    case GPU_LANGUAGE_GLSL_1_3:
        return "#version 130";
    case GPU_LANGUAGE_GLSL_4_0:
        return "#version 400 core";
    case GPU_LANGUAGE_GLSL_ES_1_0:
        return "#version 100";
    case GPU_LANGUAGE_GLSL_ES_3_0:
        return "#version 300 es";
    case GPU_LANGUAGE_CG:
    case GPU_LANGUAGE_HLSL_DX11:
    case LANGUAGE_OSL_1:
    default:
        // These are all impossible in OpenGL contexts.
        // The shader will be unusable, so let's throw
        throw Exception("Invalid shader language for OpenGLBuilder");
    }
}

unsigned OpenGLBuilder::buildProgram(const std::string & clientShaderProgram, bool standaloneShader, QOpenGLContext *context)
{
    const std::string shaderCacheID = m_shaderDesc->getCacheID();
    if(shaderCacheID!=m_shaderCacheID)
    {
        if(m_fragShader)
        {
            glDetachShader(m_program, m_fragShader);
            glDeleteShader(m_fragShader);
        }

        std::ostringstream oss;
        oss  << getGLSLVersionString() << std::endl
             << (!standaloneShader ? m_shaderDesc->getShaderText() : "") << std::endl
             << clientShaderProgram << std::endl;

        if(m_verbose)
        {
            std::cout << "\nGPU Shader Program:\n\n"
                      << oss.str()
                      << "\n\n"
                      << std::flush;
        }

        m_fragShader = CompileShaderText(GL_FRAGMENT_SHADER, oss.str().c_str());

        LinkShaders(m_program, m_fragShader);
        m_shaderCacheID = shaderCacheID;

        linkAllUniforms(context);
    }

    return m_program;
}

void OpenGLBuilder::useProgram()
{
    glUseProgram(m_program);
}

unsigned OpenGLBuilder::getProgramHandle()
{
    return m_program;
}

unsigned OpenGLBuilder::GetTextureMaxWidth()
{
    // Arbitrary huge number only to find the limit.
    static unsigned maxTextureSize = 256 * 1024;

    CheckStatus();

    unsigned w = maxTextureSize;
    unsigned h = 1;

    while(w>1)
    {
        glTexImage2D(GL_PROXY_TEXTURE_2D, 0,
                     GL_RGB32F_ARB,
                     w, h, 0,
                     GL_RGB, GL_FLOAT, NULL);

        bool texValid = true;
        GLenum glErr = GL_NO_ERROR;

        while((glErr=glGetError()) != GL_NO_ERROR)
        {
            if(glErr==GL_INVALID_VALUE)
            {
                texValid = false;
            }
        }

#ifndef __APPLE__
        //
        // In case of Linux, if glTexImage2D() succeeds
        //  glGetTexLevelParameteriv() could fail.
        //
        // In case of OSX, glTexImage2D() will provide the right result,
        //  and glGetTexLevelParameteriv() always fails.
        //  So do not try glGetTexLevelParameteriv().
        //
        if(texValid)
        {
            GLint format = 0;
            glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
                                     GL_TEXTURE_COMPONENTS, &format);

            texValid = texValid && (GL_RGB32F_ARB==format);

            while((glErr=glGetError()) != GL_NO_ERROR);
        }
#endif

        if(texValid) break;

        w = w >> 1;
        h = h << 1;
    }

    if(w==1)
    {
        throw Exception("Maximum texture size unknown");
    }

    CheckStatus();

    return w;
}

} // namespace OIIO_OCIO
