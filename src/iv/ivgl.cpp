/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <cmath>

#include <ImathFun.h>

#include <boost/foreach.hpp>

#include "imageviewer.h"
#include "dassert.h"
#include "strutil.h"
#include "timer.h"


#define GLERRPRINT(msg)                                           \
    for (GLenum err = glGetError();  err != GL_NO_ERROR;  err = glGetError()) \
        std::cerr << "GL error " << msg << " " << (int)err << "\n";      \




IvGL::IvGL (QWidget *parent, ImageViewer *viewer)
    : QGLWidget(parent), m_viewer (viewer)
{
}



IvGL::~IvGL ()
{
}



void
IvGL::initializeGL ()
{
    std::cerr << "initializeGL\n";
    glClearColor (1.0, 0.0, 0.0, 1.0);
//    object = makeObject();
    glShadeModel(GL_FLAT);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    glGenTextures (1, &m_texid);
//    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_texid);
    unsigned char pix[4] = {55, 55, 255, 255};
#if 1
    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  4 /*internal format - color components */,
                  1 /*width*/, 1 /*height*/, 0 /*border width*/,
                  GL_RGBA /*type - GL_RGB, GL_RGBA, GL_LUMINANCE */,
                  GL_UNSIGNED_BYTE /*format - GL_FLOAT */,
                  (const GLvoid *)pix /*data*/);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif

    GLERRPRINT ("bind tex 1");
#if 0
    GLuint textures[] = { GL_TEXTURE0 };
    GLboolean residences[] = { false };
    bool ok = glAreTexturesResident (0, textures, residences);
    GLERRPRINT ("bind tex 1");
    std::cerr << "Resident? " << (int)residences[0] << "\n";
#endif

    m_shader_program = glCreateProgram ();
    std::cerr << "shader program = " << (int)m_shader_program << "\n";

    static const GLchar *vertex_source = 
        "varying vec2 vTexCoord;\n"
        "void main ()\n"
        "{\n"
	"    vTexCoord = gl_MultiTexCoord0.xy;\n"
	"    gl_Position = ftransform();\n"
        "}\n";
    m_vertex_shader = glCreateShader (GL_VERTEX_SHADER);
    glShaderSource (m_vertex_shader, 1, &vertex_source, NULL);
    glCompileShader (m_vertex_shader);
    GLint status;
    glGetShaderiv (m_vertex_shader, GL_COMPILE_STATUS, &status);
    std::cerr << "vertex shader compile status: " << status << "\n";
    glAttachShader (m_shader_program, m_vertex_shader);
    GLERRPRINT ("vertex shader");

    static const GLchar *fragment_source = 
        "uniform sampler2D imgtex;\n"
        "varying vec2 vTexCoord;\n"
        "uniform float test;\n"
        "void main ()\n"
        "{\n"
        "    gl_FragColor = texture2D (imgtex, vTexCoord);\n"
//        "    gl_FragColor = vec4(test, 1, 0, 1);\n"
        "}\n";
    m_fragment_shader = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (m_fragment_shader, 1, &fragment_source, NULL);
    glCompileShader (m_fragment_shader);
    glGetShaderiv (m_fragment_shader, GL_COMPILE_STATUS, &status);
    std::cerr << "fragment shader compile status: " << status << "\n";
    char buf[10000];
    buf[0] = 0;
    GLsizei len;
    glGetShaderInfoLog (m_fragment_shader, sizeof(buf), &len, buf);
    std::cerr << "compile log:\n" << buf << "---\n";
    glAttachShader (m_shader_program, m_fragment_shader);
    GLERRPRINT ("fragment shader");

    glLinkProgram (m_shader_program);
    GLERRPRINT ("link");
    GLint linked, attached_shaders;
    glGetProgramiv (m_shader_program, GL_LINK_STATUS, &linked);
    std::cerr << "linked? " << (int)linked << "\n";
    GLERRPRINT ("check link");
    glGetProgramiv (m_shader_program, GL_ATTACHED_SHADERS, &attached_shaders);
    std::cerr << "attached shaders: " << (int)attached_shaders << "\n";

    GLint samplerloc = glGetUniformLocation (m_shader_program, "imgtex");
    GLERRPRINT ("use tex 1");
    glUniform1i (samplerloc, m_texid /*texture sampler number*/);
    GLERRPRINT ("use tex 2");
}



void
IvGL::resizeGL (int w, int h)
{
    std::cerr << "resizeGL " << w << ' ' << h << "\n";
    GLERRPRINT ("resizeGL entry");
//    int side = qMin(w, h);
//    glViewport ((w - side) / 2, (h - side) / 2, side, side);
    glViewport (0, 0, w, h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (-w/2.0, w/2.0, -h/2.0, h/2.0, 0, 10);
    glMatrixMode (GL_MODELVIEW);
    GLERRPRINT ("resizeGL exit");
}



void
IvGL::paintGL ()
{
    std::cerr << "paintGL\n";
    GLERRPRINT ("paintGL entry");
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (! m_viewer || ! m_viewer->cur())
        return;
    glLoadIdentity ();
    glTranslatef (0.5, 0.5, -1.0);
    glScalef (m_viewer->zoom(), m_viewer->zoom(), 1);
    IvImage *img = m_viewer->cur();
    const ImageIOFormatSpec &spec (img->spec());
    glScalef (spec.width, spec.height, 1);

#if 0
    GLint testloc = glGetUniformLocation (m_shader_program, "test");
    GLERRPRINT ("getloc 1");
    glUniform1f (testloc, 0.5 /*texture sampler number*/);
    GLERRPRINT ("getloc 2");
#endif

    GLERRPRINT ("before use program");
    glUseProgram (m_shader_program);
    GLERRPRINT ("use program");

    glBegin (GL_QUAD_STRIP);
    glTexCoord2f (0, 0);
    glVertex3f (-0.5f, 0.5f, 1.0f);
    glTexCoord2f (1, 0);
    glVertex3f (0.5f, 0.5f, 1.0f);
    glTexCoord2f (0, 1);
    glVertex3f (-0.5f, -0.5f, 1.0f);
    glTexCoord2f (1, 1);
    glVertex3f (0.5f, -0.5f, 1.0f);
    glEnd ();
}



void
IvGL::update (IvImage *img)
{
    const ImageIOFormatSpec &spec (img->spec());
//    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_texid);
    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  spec.nchannels /*internal format - color components */,
                  spec.width /*width*/, spec.height /*height*/,
                  0 /*border width*/,
                  spec.nchannels == 4 ? GL_RGBA : GL_RGB /*type*/,
                  GL_UNSIGNED_BYTE /*format - GL_FLOAT */,
                  (const GLvoid *)img->scanline(0) /*data*/);
}



void
IvGL::zoom (float z)
{
    IvImage *img = m_viewer->cur();
    if (! img)
        return;
    const ImageIOFormatSpec &spec (img->spec());
    std::cerr << "resizing to " << 
        (spec.width * z) << ' ' << (spec.height * z) << "\n";
    resize (spec.width * z, spec.height * z);
    // Update the texture
    repaint (0, 0, spec.width * z, spec.height * z);
//    update (img);
}
