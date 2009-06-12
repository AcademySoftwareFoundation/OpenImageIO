/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <iostream>

// This needs to be included before GL.h
// (which is included by QtOpenGL and QGLFormat)
#include <glew.h>

#include <half.h>
#include <ImathFun.h>
#include <QGLFormat>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/compare.hpp>

#include "imageviewer.h"
#include "strutil.h"
#include "fmath.h"




#define GLERRPRINT(msg)                                           \
    for (GLenum err = glGetError();  err != GL_NO_ERROR;  err = glGetError()) \
        std::cerr << "GL error " << msg << " " << (int)err <<  " - " << (const char *)gluErrorString(err) << "\n";      \



IvGL::IvGL (QWidget *parent, ImageViewer &viewer)
    : QGLWidget(parent), m_viewer(viewer), 
      m_shaders_created(false), m_tex_created(false),
      m_zoom(1.0), m_centerx(0), m_centery(0), m_dragging(false),
      m_use_shaders(false), m_use_halffloat(false), m_use_float(false),
      m_use_srgb(false), m_texture_height(1), m_texture_width(1),
      m_shaders_using_extensions(false), m_use_npot_texture(false)
{
#if 0
    QGLFormat format;
    format.setRedBufferSize (32);
    format.setGreenBufferSize (32);
    format.setBlueBufferSize (32);
    format.setAlphaBufferSize (32);
    format.setDepth (true);
    setFormat (format);
#endif
    setMouseTracking (true);
}



IvGL::~IvGL ()
{
}



void
IvGL::initializeGL ()
{
    GLenum glew_error = glewInit ();
    if (glew_error != GLEW_OK) {
        std::cerr << "GLEW init error " << glewGetErrorString (glew_error) << "\n";
    }

    glClearColor (0.05, 0.05, 0.05, 1.0);
    glShadeModel (GL_FLAT);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glEnable (GL_ALPHA_TEST);
    glEnable (GL_BLEND);
//    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
//    glEnable (GL_TEXTURE_2D);
    // Make sure initial matrix is identity (returning to this stack level loads
    // back this matrix).
    glLoadIdentity();
    // There's this small detail in the OpenGL 2.1 (probably earlier versions
    // too) spec:
    //
    // (For TexImage3D, TexImage2D and TexImage1D):
    // The values of UNPACK ROW LENGTH and UNPACK ALIGNMENT control the row-to-
    // row spacing in these images in the same manner as DrawPixels.
    //
    // UNPACK_ALIGNMENT has a default value of 4 according to the spec. Which
    // means that it was expecting images to be Aligned to 4-bytes, and making
    // several odd "skew-like effects" in the displayed images. Setting the
    // alignment to 1 byte fixes this problems.
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);          

    // here we check what OpenGL extensions are available, and take action
    // if needed
    check_gl_extensions ();

    create_textures ();

    create_shaders ();
}



void
IvGL::create_textures (void)
{
    if (m_tex_created)
        return;

    glGenTextures (1, &m_texid);
    GLERRPRINT ("gen textures");
    glBindTexture (GL_TEXTURE_2D, m_texid);
    GLERRPRINT ("bind tex");
    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  4 /*internal format - color components */,
                  1 /*width*/, 1 /*height*/, 0 /*border width*/,
                  GL_RGBA /*type - GL_RGB, GL_RGBA, GL_LUMINANCE */,
                  GL_FLOAT /*format - GL_FLOAT */,
                  NULL /*data*/);
    GLERRPRINT ("tex image 2d");
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    GLERRPRINT ("After tex parameters");

#if 0
    GLuint textures[] = { GL_TEXTURE0 };
    GLboolean residences[] = { false };
    bool ok = glAreTexturesResident (0, textures, residences);
    GLERRPRINT ("bind tex 1");
    std::cerr << "Resident? " << (int)residences[0] << "\n";
#endif

    m_tex_created = true;
}



void
IvGL::create_shaders (void)
{
    static const GLchar *vertex_source = 
        "varying vec2 vTexCoord;\n"
        "void main ()\n"
        "{\n"
        "    vTexCoord = gl_MultiTexCoord0.xy;\n"
        "    gl_Position = ftransform();\n"
        "}\n";

    static const GLchar *fragment_source = 
        "uniform sampler2D imgtex;\n"
        "varying vec2 vTexCoord;\n"
        "uniform float gain;\n"
        "uniform float gamma;\n"
        "uniform int channelview;\n"
        "uniform int imgchannels;\n"
        "uniform int pixelview;\n"
        "uniform int linearinterp;\n"
        "uniform int width;\n"
        "uniform int height;\n"
        "void main ()\n"
        "{\n"
        "    vec2 st = vTexCoord;\n"
        "    float black = 0.0;\n"
        "    if (pixelview != 0 || linearinterp == 0) {\n"
        "        vec2 wh = vec2(width,height);\n"
        "        vec2 onehalf = vec2(0.5,0.5);\n"
        "        vec2 st_res = st * wh /* + onehalf */ ;\n"
        "        vec2 st_pix = floor (st_res);\n"
        "        vec2 st_rem = st_res - st_pix;\n"
        "        st = (st_pix + onehalf) / wh;\n"
        "        if (pixelview != 0) {\n"
        "            if (st.x < 0.0 || st.x >= 1.0 || \n"
        "                    st.y < 0.0 || st.y >= 1.0 || \n"
        "                    st_rem.x < 0.05 || st_rem.x >= 0.95 || \n"
        "                    st_rem.y < 0.05 || st_rem.y >= 0.95)\n"
        "                black = 1.0;\n"
        "        }\n"
        "    }\n"
        "    vec4 C = texture2D (imgtex, st);\n"
        "    C = mix (C, vec4(0.05,0.05,0.05,1.0), black);\n"
        "    if (pixelview != 0)\n"
        "        C.a = 1.0;\n"
        "    if (imgchannels == 1)\n"
        "        C = C.xxxx;\n"
        "    if (channelview == -1) {\n"
        "    }\n"
        "    else if (channelview == 0)\n"
        "        C.xyz = C.xxx;\n"
        "    else if (channelview == 1)\n"
        "        C.xyz = C.yyy;\n"
        "    else if (channelview == 2)\n"
        "        C.xyz = C.zzz;\n"
        "    else if (channelview == 3)\n"
        "        C.xyz = C.www;\n"
        "    else if (channelview == -2) {\n"
        "        float lum = dot (C.xyz, vec3(0.2126, 0.7152, 0.0722));\n"
        "        C.xyz = vec3 (lum, lum, lum);\n"
        "    }\n"
        "    C.xyz *= gain;\n"
        "    float invgamma = 1.0/gamma;\n"
        "    C.xyz = pow (C.xyz, vec3 (invgamma, invgamma, invgamma));\n"
        "    gl_FragColor = C;\n"
        "}\n";

    if (!m_use_shaders) {
        std::cerr << "Not using shaders!\n";
        return;
    }
    if (m_shaders_created)
        return;

    // When using extensions to support shaders, we need to load the function
    // entry points (which is actually done by GLEW) and then call them. So
    // we have to get the functions through the right symbols otherwise
    // extension-based shaders won't work.
    if (m_shaders_using_extensions) {
        m_shader_program = glCreateProgramObjectARB ();
    }
    else {
        m_shader_program = glCreateProgram ();
    }
    GLERRPRINT ("create progam");

    // This holds the compilation status
    GLint status;

    if (m_shaders_using_extensions) {
        m_vertex_shader = glCreateShaderObjectARB (GL_VERTEX_SHADER_ARB);
        glShaderSourceARB (m_vertex_shader, 1, &vertex_source, NULL);
        glCompileShaderARB (m_vertex_shader);
        glGetObjectParameterivARB (m_vertex_shader,
                GL_OBJECT_COMPILE_STATUS_ARB, &status);
    } else {
        m_vertex_shader = glCreateShader (GL_VERTEX_SHADER);
        glShaderSource (m_vertex_shader, 1, &vertex_source, NULL);
        glCompileShader (m_vertex_shader);
        glGetShaderiv (m_vertex_shader, GL_COMPILE_STATUS, &status);
    }
    if (! status) {
        // FIXME: How to handle this error?
        std::cerr << "vertex shader compile status: failed\n";
    }
    if (m_shaders_using_extensions) {
        glAttachObjectARB (m_shader_program, m_vertex_shader);
    } else {
        glAttachShader (m_shader_program, m_vertex_shader);
    }
    GLERRPRINT ("After attach vertex shader.");

    if (m_shaders_using_extensions) {
        m_fragment_shader = glCreateShaderObjectARB (GL_FRAGMENT_SHADER_ARB);
        glShaderSourceARB (m_fragment_shader, 1, &fragment_source, NULL);
        glCompileShaderARB (m_fragment_shader);
        glGetObjectParameterivARB (m_fragment_shader,
                GL_OBJECT_COMPILE_STATUS_ARB, &status);
    } else {
        m_fragment_shader = glCreateShader (GL_FRAGMENT_SHADER);
        glShaderSource (m_fragment_shader, 1, &fragment_source, NULL);
        glCompileShader (m_fragment_shader);
        glGetShaderiv (m_fragment_shader, GL_COMPILE_STATUS, &status);
    }
    if (! status) {
        std::cerr << "fragment shader compile status: " << status << "\n";
        char buf[10000];
        buf[0] = 0;
        GLsizei len;
        if (m_shaders_using_extensions) {
            glGetInfoLogARB (m_fragment_shader, sizeof(buf), &len, buf);
        } else {
            glGetShaderInfoLog (m_fragment_shader, sizeof(buf), &len, buf);
        }
        std::cerr << "compile log:\n" << buf << "---\n";
        // FIXME: How to handle this error?
    }
    if (m_shaders_using_extensions) {
        glAttachObjectARB (m_shader_program, m_fragment_shader);
    } else {
        glAttachShader (m_shader_program, m_fragment_shader);
    }
    GLERRPRINT ("After attach fragment shader");

    if (m_shaders_using_extensions) {
        glLinkProgramARB (m_shader_program);
    } else {
        glLinkProgram (m_shader_program);
    }
    GLERRPRINT ("link");
    GLint linked, attached_shaders;
    if (m_shaders_using_extensions) {
        glGetObjectParameterivARB (m_shader_program,
                GL_OBJECT_LINK_STATUS_ARB, &linked);
    } else {
        glGetProgramiv (m_shader_program, GL_LINK_STATUS, &linked);
    }
    if (! linked) {
        std::cerr << "NOT LINKED\n";
        // FIXME: How to handle this error?
    }

    m_shaders_created = true;

}



void
IvGL::resizeGL (int w, int h)
{
    GLERRPRINT ("resizeGL entry");
    glViewport (0, 0, w, h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (-w/2.0, w/2.0, -h/2.0, h/2.0, 0, 10);
    // Main GL viewport is set up for orthographic view centered at
    // (0,0) and with width and height equal to the window dimensions IN
    // PIXEL UNITS.
    glMatrixMode (GL_MODELVIEW);
    GLERRPRINT ("resizeGL exit");
}



static void
gl_rect (float xmin, float ymin, float xmax, float ymax, float z = 0,
         float smin = 0, float tmin = 0, float smax = 1, float tmax = 1,
         int rotate = 0)
{
    float tex[] = { smin, tmin, smax, tmin, smax, tmax, smin, tmax };
    glBegin (GL_POLYGON);
    glTexCoord2f (tex[(0+2*rotate)&7], tex[(1+2*rotate)&7]);
    glVertex3f (xmin,  ymin, z);
    glTexCoord2f (tex[(2+2*rotate)&7], tex[(3+2*rotate)&7]);
    glVertex3f (xmax,  ymin, z);
    glTexCoord2f (tex[(4+2*rotate)&7], tex[(5+2*rotate)&7]);
    glVertex3f (xmax, ymax, z);
    glTexCoord2f (tex[(6+2*rotate)&7], tex[(7+2*rotate)&7]);
    glVertex3f (xmin, ymax, z);
    glEnd ();
}



void
IvGL::paintGL ()
{
//    std::cerr << "paintGL " << m_viewer.current_image() << " with zoom " << m_zoom << "\n";
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (! m_viewer.cur())
        return;
 
    IvImage *img = m_viewer.cur();
    const ImageSpec &spec (img->spec());
    float z = m_zoom;

    glPushMatrix ();
    // Transform is now same as the main GL viewport -- window pixels as
    // units, with (0,0) at the center of the visible unit.
    glTranslatef (0, 0, -5.0);
    // Pushed away from the camera 5 units.
    glScalef (1, -1, 1);
    // Flip y, because OGL's y runs from bottom to top.
    glScalef (z, z, 1);
    // Scaled by zoom level.  So now xy units are image pixels as
    // displayed at the current zoom level, with the origin at the
    // center of the visible window.
    glTranslatef (-m_centerx, -m_centery, 0.0f);
    // Recentered so that the pixel space (m_centerx,m_centery) position is
    // at the center of the visible window.

    // Handle orientation
    float xmin = spec.x;
    float xmax = spec.x + spec.width;
    float ymin = spec.y;
    float ymax = spec.y + spec.height;
    float smin = 0, smax = spec.width/float (m_texture_width);
    float tmin = 0, tmax = spec.height/float (m_texture_height);
    int orient = img->orientation();
    int rotate = 0;
    if (orient != 1) {
        if (orient == 2 || orient == 3 || orient == 5 || orient == 8)
            std::swap (xmin, xmax);
        if (orient == 3 || orient == 4)
            std::swap (ymin, ymax);
        if (orient == 5 || orient == 8) {
            float x0 = xmin, x1 = xmax, y0 = ymin, y1 = ymax;
            xmin = y1;
            xmax = y0;
            ymin = x0;
            ymax = x1;
            rotate = 3;
        } else if (orient == 6 || orient == 7) {
            float x0 = xmin, x1 = xmax, y0 = ymin, y1 = ymax;
            if (orient == 6) {
                xmin = y1;
                xmax = y0;
            } else {
                xmin = y0;
                xmax = y1;
            }
            ymin = x1;
            ymax = x0;
            rotate = 1;
        }
    }
    useshader ();

    gl_rect (xmin, ymin, xmax, ymax, 0, smin, tmin, smax, tmax, rotate);

    glPopMatrix ();

    if (m_viewer.pixelviewOn()) {
        paint_pixelview ();
    }
}



void
IvGL::shadowed_text (float x, float y, float z, const std::string &s,
                     const QFont &font)
{
    QString q (s.c_str());
#if 0
    glColor4f (0, 0, 0, 1);
    const int b = 2;  // blur size
    for (int i = -b;  i <= b;  ++i)
        for (int j = -b;  j <= b;  ++j)
            renderText (x+i, y+j, q, font);
#endif
    glColor4f (1, 1, 1, 1);
    renderText (x, y, z, q, font);
}



void
IvGL::paint_pixelview ()
{
    // ncloseuppixels is the number of big pixels (in each direction)
    // visible in our closeup window.
    const int ncloseuppixels = 9;   // How many pixels to show in each dir
    // closeuppixelzoom is the zoom factor we use for closeup pixels --
    // i.e. one image pixel will appear in the closeup window as a 
    // closeuppixelzoom x closeuppixelzoom square.
    const int closeuppixelzoom = 24;
    // closeupsize is the size, in pixels, of the closeup window itself --
    // just the number of pixels times the width of each closeup pixel.
    const int closeupsize = ncloseuppixels * closeuppixelzoom;

    IvImage *img = m_viewer.cur();
    const ImageSpec &spec (img->spec());

    // (xw,yw) are the window coordinates of the mouse.
    int xw, yw;
    m_viewer.glwin->get_focus_window_pixel (xw, yw);

    // (xp,yp) are the image-space [0..res-1] position of the mouse.
    int xp, yp;
    get_focus_image_pixel (xp, yp);

    glPushMatrix ();
    // Transform is now same as the main GL viewport -- window pixels as
    // units, with (0,0) at the center of the visible window.

    glTranslatef (0, 0, -1);
    // Pushed away from the camera 1 unit.  This makes the pixel view
    // elements closer to the camera than the main view.

    if (m_viewer.pixelviewFollowsMouse()) {
        // Display closeup overtop mouse -- translate the coordinate system
        // so that it is centered at the mouse position.
        glTranslatef (xw - width()/2, -yw + height()/2, 0);
    } else {
        // Display closeup in upper left corner -- translate the coordinate
        // system so that it is centered near the upper left of the window.
        glTranslatef (closeupsize*0.5f + 5 - width()/2,
                      -closeupsize*0.5f - 5 + height()/2, 0);
    }
    // In either case, the GL coordinate system is now scaled to window
    // pixel units, and centered on the middle of where the closeup
    // window is going to appear.  All other coordinates from here on
    // (in this procedure) should be relative to the closeup window center.

    // This square is the closeup window itself
    //
    glPushAttrib (GL_ENABLE_BIT | GL_TEXTURE_BIT);
    useshader (true);
    if (! (xp >= 0 && xp < img->oriented_width() && yp >= 0 && yp < img->oriented_height())) {
        glDisable (GL_TEXTURE_2D);
        glColor3f (0.1,0.1,0.1);
    }
    if (! m_use_shaders) {
        glDisable (GL_BLEND);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    float oriented_tex_width = (img->orientation() <= 4 ? m_texture_width : m_texture_height);
    float oriented_tex_height = (img->orientation() <= 4 ? m_texture_height : m_texture_width);
    float xtexsize = 0.5 * (float)ncloseuppixels / oriented_tex_width;
    float ytexsize = 0.5 * (float)ncloseuppixels / oriented_tex_height;
    // Make (x,y) be the image space NDC coords of the mouse.
    float x = (xp+0.5f) / (/* ? z * */ oriented_tex_width);
    float y = (yp+0.5f) / (/* ? z * */ oriented_tex_height);

    gl_rect (-0.5f*closeupsize, 0.5f*closeupsize,
            0.5f*closeupsize, -0.5f*closeupsize, 0,
            x - xtexsize, y - ytexsize, x + xtexsize, y + ytexsize);
    glPopAttrib ();

    // Draw a second window, slightly behind the closeup window, as a
    // backdrop.  It's partially transparent, having the effect of
    // darkening the main image view beneath the closeup window.  It
    // extends slightly out from the closeup window (making it more
    // clearly visible), and also all the way down to cover the area
    // where the text will be printed, so it is very readable.
    const int yspacing = 18;

    glPushAttrib (GL_ENABLE_BIT);
    glDisable (GL_TEXTURE_2D);
    if (m_use_shaders) {
        // Disable shaders for this.
        gl_use_program (0);
    }
    float extraspace = yspacing * (1 + spec.nchannels) + 4;
    glColor4f (0.1, 0.1, 0.1, 0.5);
    gl_rect (-0.5f*closeupsize-2, 0.5f*closeupsize+2,
             0.5f*closeupsize+2, -0.5f*closeupsize - extraspace, -0.1);

    if (xp >= 0 && xp < img->oriented_width() && yp >= 0 && yp < img->oriented_height()) {
        // Now we print text giving the mouse coordinates and the numerical
        // values of the pixel that the mouse is over.
        QFont font;
        font.setFixedPitch (true);
//        std::cerr << "pixel size " << font.pixelSize() << "\n";
//    font.setPixelSize (16);
//    font.setFixedPitch (20);
//    bgfont.setPixelSize (20);
        float *fpixel = (float *) alloca (spec.nchannels*sizeof(float));
        int textx = - closeupsize/2 + 4;
        int texty = - closeupsize/2 - yspacing;
        std::string s = Strutil::format ("(%d, %d)", xp+spec.x, yp+spec.y);
        shadowed_text (textx, texty, 0.0f, s, font);
        texty -= yspacing;
        img->getpixel (xp+spec.x, yp+spec.y, fpixel);
        for (int i = 0;  i < spec.nchannels;  ++i) {
            switch (spec.format.basetype) {
            case TypeDesc::UINT8 : {
                ImageBuf::ConstIterator<unsigned char,unsigned char> p (*img, xp+spec.x, yp+spec.y);
                s = Strutil::format ("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(),
                                     (int)(p[i]), fpixel[i]);
                }
                break;
            case TypeDesc::UINT16 : {
                ImageBuf::ConstIterator<unsigned short,unsigned short> p (*img, xp+spec.x, yp+spec.y);
                s = Strutil::format ("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(),
                                     (int)(p[i]), fpixel[i]);
                }
                break;
            default:  // everything else, treat as float
                s = Strutil::format ("%s: %5.3f",
                                     spec.channelnames[i].c_str(), fpixel[i]);
            }
            shadowed_text (textx, texty, 0.0f, s, font);
            texty -= yspacing;
        }
    }

    glPopAttrib ();
    glPopMatrix ();
}



void
IvGL::useshader (bool pixelview)
{
    IvImage *img = m_viewer.cur();
    if (! img)
        return;

    //glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);
    //glBindTexture (GL_TEXTURE_2D, m_texid);

    if (!m_use_shaders) {
        glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        if (m_viewer.linearInterpolation ()) {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        return;
    }

    const ImageSpec &spec (img->spec());

    gl_use_program (m_shader_program);
    GLERRPRINT ("After use program");

    GLint loc;

    loc = gl_get_uniform_location ("imgtex");
    // This is the texture unit, not the texture object
    gl_uniform (loc, 0);

    loc = gl_get_uniform_location ("gain");

    float gain = powf (2.0, img->exposure ());
    gl_uniform (loc, gain);

    loc = gl_get_uniform_location ("gamma");
    gl_uniform (loc, img->gamma ());

    loc = gl_get_uniform_location ("channelview");
    gl_uniform (loc, m_viewer.current_channel ());

    loc = gl_get_uniform_location ("imgchannels");
    gl_uniform (loc, spec.nchannels);

    loc = gl_get_uniform_location ("pixelview");
    gl_uniform (loc, pixelview);

    loc = gl_get_uniform_location ("linearinterp");
    gl_uniform (loc, m_viewer.linearInterpolation ());

    loc = gl_get_uniform_location ("width");
    gl_uniform (loc, m_texture_width);

    loc = gl_get_uniform_location ("height");
    gl_uniform (loc, m_texture_height);
    GLERRPRINT ("After settting uniforms");
}



void
IvGL::update (IvImage *img)
{
    if (! img)
        return;

//    std::cerr << "update image\n";

    if (! is_glsl_capable ()) {
        img->select_channel (m_viewer.current_channel());
        if (img->exposure () != 0.0 || img->gamma () != 1.0) {
            img->apply_corrections ();
        }
    }

    const ImageSpec &spec (img->spec());
//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
//    glBindTexture (GL_TEXTURE_2D, m_texid);

    bool srgb = false;
    if (m_use_srgb && spec.linearity == ImageSpec::sRGB) {
        srgb = true;
    }
    bool format_float = false;
    if (m_use_float && spec.format.basetype == TypeDesc::FLOAT) {
        format_float = true;
    }
    bool format_half = false;
    if (m_use_float && spec.format.basetype == TypeDesc::HALF) {
        format_half = true;
    }

    GLenum glformat = GL_RGB;
    GLint glinternalformat = spec.nchannels;
    if (spec.nchannels == 1) {
        glformat = GL_LUMINANCE;
        if (srgb) {
            glinternalformat = GL_SLUMINANCE;
        } else if (format_float) {
            glinternalformat = GL_LUMINANCE32F_ARB;
        } else if (format_half) {
            glinternalformat = GL_LUMINANCE16F_ARB;
        }
    } else if (spec.nchannels == 2) {
        glformat = GL_LUMINANCE_ALPHA;
        if (srgb) {
            glinternalformat = GL_SLUMINANCE_ALPHA;
        } else if (format_float) {
            glinternalformat = GL_LUMINANCE_ALPHA32F_ARB;
        } else if (format_half) {
            glinternalformat = GL_LUMINANCE_ALPHA16F_ARB;
        }
    } else if (spec.nchannels == 3) {
        glformat = GL_RGB;
        if (srgb) {
            glinternalformat = GL_SRGB;
        } else if (format_float) {
            glinternalformat = GL_RGB32F_ARB;
        } else if (format_half) {
            glinternalformat = GL_RGB16F_ARB;
        }
    } else if (spec.nchannels == 4) {
        glformat = GL_RGBA;
        if (srgb) {
            glinternalformat = GL_SRGB_ALPHA;
        } else if (format_float) {
            glinternalformat = GL_RGBA32F_ARB;
        } else if (format_half) {
            glinternalformat = GL_RGBA16F_ARB;
        }
    } else {
        //FIXME: What to do here?
        std::cerr << "I don't know how to handle more than 4 channels\n";
    }

    GLenum gltype = GL_UNSIGNED_BYTE;
    switch (spec.format.basetype) {
    case TypeDesc::FLOAT  : gltype = GL_FLOAT;          break;
    case TypeDesc::HALF   : if (m_use_halffloat) {
                                gltype = GL_HALF_FLOAT_ARB;
                            } else {
                                // If we reach here then something really wrong
                                // happened: When half-float is not supported,
                                // the image should be loaded as UINT8 (no GLSL
                                // support) or FLOAT (GLSL support).
                                // See IvImage::loadCurrentImage()
                                std::cerr << "Tried to load an unsupported half-float image.\n";
                            }
                            break;
    case TypeDesc::INT8   : gltype = GL_BYTE;           break;
    case TypeDesc::UINT8  : gltype = GL_UNSIGNED_BYTE;  break;
    case TypeDesc::INT16  : gltype = GL_SHORT;          break;
    case TypeDesc::UINT16 : gltype = GL_UNSIGNED_SHORT; break;
    case TypeDesc::INT    : gltype = GL_INT;            break;
    case TypeDesc::UINT   : gltype = GL_UNSIGNED_INT;   break;
    default:
        gltype = GL_UNSIGNED_BYTE;  // punt
        break;
    }

    if (! m_use_npot_texture) {
        m_texture_width = pow2roundup(spec.width);
        m_texture_height= pow2roundup(spec.height);
    }
    else {
        m_texture_width = spec.width;
        m_texture_height= spec.height;
    }

    // Copy the imagebuf pixels we need, that's the only way we can do
    // it safely once ImageBuf has a cache underneath and the whole image
    // may not be resident at once.
    // FIXME -- when we render in "tiles", this will copy a tile rather 
    // than the whole image.
    std::vector<unsigned char> buf;
    buf.resize (spec.width * spec.height * spec.pixel_bytes());
    img->copy_pixels (spec.x, spec.x+spec.width, spec.y, spec.y+spec.height,
                      spec.format, &buf[0]);
    GLvoid *full_texture_data = NULL;
    if (m_texture_width == spec.width && m_texture_height == spec.height) {
        full_texture_data = (GLvoid *) &buf[0];
    }

    //std::cerr << "Width: " << spec.width << ". Height: " << spec.height << "\n";
    //std::cerr << "Texture width: " << m_texture_width << ". Texture height: " << m_texture_height << std::endl;
    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  glinternalformat,
                  m_texture_width,  m_texture_height,
                  0 /*border width*/,
                  glformat, gltype, 
                  full_texture_data /*data*/);

    if (! full_texture_data) {
        glTexSubImage2D(GL_TEXTURE_2D, 0 /*mip level*/,
                        0, 0 /* x, y within image */,
                        spec.width, spec.height /*width, height of patch*/,
                        glformat, gltype,
                        (GLvoid *)&buf[0]);
    }
}



void
IvGL::view (float xcenter, float ycenter, float zoom, bool redraw)
{
    m_centerx = xcenter;
    m_centery = ycenter;
    m_zoom = zoom;

    IvImage *img = m_viewer.cur();
    if (img) {
        clamp_view_to_window ();
//        repaint (0, 0, img->oriented_width(), img->oriented_height());     // Update the texture
    } else {
//        repaint (0, 0, width(), height());
    }
    if (redraw)
        trigger_redraw ();
}



void
IvGL::pan (float dx, float dy)
{
    center (m_centerx + dx, m_centery + dy);
}



void
IvGL::remember_mouse (const QPoint &pos)
{
    m_mousex = pos.x();
    m_mousey = pos.y();
}



void
IvGL::clamp_view_to_window ()
{
    IvImage *img = m_viewer.cur();
    if (! img)
        return;
    int w = width(), h = height();
    float zoomedwidth  = m_zoom * img->oriented_full_width();
    float zoomedheight = m_zoom * img->oriented_full_height();
#if 0
    float left    = m_centerx - 0.5 * ((float)w / m_zoom);
    float top     = m_centery - 0.5 * ((float)h / m_zoom);
    float right   = m_centerx + 0.5 * ((float)w / m_zoom);
    float bottom  = m_centery + 0.5 * ((float)h / m_zoom);
    std::cerr << "Window size is " << w << " x " << h << "\n";
    std::cerr << "Center (pixel coords) is " << m_centerx << ", " << m_centery << "\n";
    std::cerr << "Top left (pixel coords) is " << left << ", " << top << "\n";
    std::cerr << "Bottom right (pixel coords) is " << right << ", " << bottom << "\n";
#endif

    int xmin = std::min (img->oriented_x(), img->oriented_full_x());
    int xmax = std::max (img->oriented_x()+img->oriented_width(),
                         img->oriented_full_x()+img->oriented_full_width());
    int ymin = std::min (img->oriented_y(), img->oriented_full_y());
    int ymax = std::max (img->oriented_y()+img->oriented_height(),
                         img->oriented_full_y()+img->oriented_full_height());

    // Don't let us scroll off the edges
    if (zoomedwidth >= w) {
        m_centerx = Imath::clamp (m_centerx, xmin + 0.5f*w/m_zoom, xmax - 0.5f*w/m_zoom);
    } else {
        m_centerx = img->oriented_full_x() + img->oriented_full_width()/2;
    }

    if (zoomedheight >= h) {
        m_centery = Imath::clamp (m_centery, ymin + 0.5f*h/m_zoom, ymax - 0.5f*h/m_zoom);
    } else {
        m_centery = img->oriented_full_y() + img->oriented_full_height()/2;
    }
}



void
IvGL::mousePressEvent (QMouseEvent *event)
{
    remember_mouse (event->pos());
    int mousemode = m_viewer.mouseModeComboBox->currentIndex ();
    bool Alt = (event->modifiers() & Qt::AltModifier);
    m_drag_button = event->button();
    switch (event->button()) {
    case Qt::LeftButton :
        if (mousemode == ImageViewer::MouseModeZoom && !Alt)
            m_viewer.zoomIn();
        else
            m_dragging = true;
        return;
    case Qt::RightButton :
        if (mousemode == ImageViewer::MouseModeZoom && !Alt)
            m_viewer.zoomOut();
        else
            m_dragging = true;
        return;
    case Qt::MidButton :
        m_dragging = true;
        // FIXME: should this be return rather than break?
        break;
    default:
        break;
    }
    parent_t::mousePressEvent (event);
}



void
IvGL::mouseReleaseEvent (QMouseEvent *event)
{
    remember_mouse (event->pos());
    m_drag_button = Qt::NoButton;
    m_dragging = false;
    parent_t::mouseReleaseEvent (event);
}



void
IvGL::mouseMoveEvent (QMouseEvent *event)
{
    QPoint pos = event->pos();
    // FIXME - there's probably a better Qt way than tracking the button
    // myself.
    bool Alt = (event->modifiers() & Qt::AltModifier);
    int mousemode = m_viewer.mouseModeComboBox->currentIndex ();
    bool do_pan = false, do_zoom = false, do_wipe = false;
    bool do_select = false, do_annotate = false;
    switch (mousemode) {
    case ImageViewer::MouseModeZoom :
        if ((m_drag_button == Qt::MidButton) ||
            (m_drag_button == Qt::LeftButton && Alt)) {
            do_pan = true;
        } else if (m_drag_button == Qt::RightButton && Alt) {
            do_zoom = true;
        }
        break;
    case ImageViewer::MouseModePan :
        if (m_drag_button != Qt::NoButton)
            do_pan = true;
        break;
    case ImageViewer::MouseModeWipe :
        if (m_drag_button != Qt::NoButton)
            do_wipe = true;
        break;
    case ImageViewer::MouseModeSelect :
        if (m_drag_button != Qt::NoButton)
            do_select = true;
        break;
    case ImageViewer::MouseModeAnnotate :
        if (m_drag_button != Qt::NoButton)
            do_annotate = true;
        break;
    }
    if (do_pan) {
        float dx = (pos.x() - m_mousex) / m_zoom;
        float dy = (pos.y() - m_mousey) / m_zoom;
        pan (-dx, -dy);
    } else if (do_zoom) {
        float dx = (pos.x() - m_mousex);
        float dy = (pos.y() - m_mousey);
        float z = m_viewer.zoom() * (1.0 + 0.005 * (dx + dy));
        z = Imath::clamp (z, 0.01f, 256.0f);
        m_viewer.zoom (z);
        m_viewer.fitImageToWindowAct->setChecked (false);
    } else if (do_wipe) {
        // FIXME -- unimplemented
    } else if (do_select) {
        // FIXME -- unimplemented
    } else if (do_annotate) {
        // FIXME -- unimplemented
    }
    remember_mouse (pos);
    if (m_viewer.pixelviewOn())
        trigger_redraw ();
    parent_t::mouseMoveEvent (event);
}



void
IvGL::wheelEvent (QWheelEvent *event)
{
    if (event->orientation() == Qt::Vertical) {
        int degrees = event->delta() / 8;
        if (true || (event->modifiers() & Qt::AltModifier)) {
            // Holding down Alt while wheeling makes smooth zoom of small
            // increments
            float z = m_viewer.zoom();
            z *= 1.0 + 0.005*degrees;
            z = Imath::clamp (z, 0.01f, 256.0f);
            m_viewer.zoom (z);
            m_viewer.fitImageToWindowAct->setChecked (false);
        } else {
            if (degrees > 5)
                m_viewer.zoomIn ();
            else if (degrees < -5)
                m_viewer.zoomOut ();
        }
        event->accept();
    }
}



void
IvGL::get_focus_window_pixel (int &x, int &y)
{
    x = m_mousex;
    y = m_mousey;
}



void
IvGL::get_focus_image_pixel (int &x, int &y)
{
    // w,h are the dimensions of the visible window, in pixels
    int w = width(), h = height();
    float z = m_zoom;
    // left,top,right,bottom are the borders of the visible window, in 
    // pixel coordinates
    float left    = m_centerx - 0.5 * w / z;
    float top     = m_centery - 0.5 * h / z;
    float right   = m_centerx + 0.5 * w / z;
    float bottom  = m_centery + 0.5 * h / z;
    // normx,normy are the position of the mouse, in normalized (i.e. [0..1])
    // visible window coordinates.
    float normx = (float)(m_mousex + 0.5f) / w;
    float normy = (float)(m_mousey + 0.5f) / h;
    // imgx,imgy are the position of the mouse, in pixel coordinates
    float imgx = Imath::lerp (left, right, normx);
    float imgy = Imath::lerp (top, bottom, normy);
    // So finally x,y are the coordinates of the image pixel (on [0,res-1])
    // underneath the mouse cursor.
    //FIXME: Shouldn't this take image rotation into account?
    x = imgx;
    y = imgy;
#if 0
    std::cerr << "get_focus_pixel\n";
    std::cerr << "    mouse window pixel coords " << m_mousex << ' ' << m_mousey << "\n";
    std::cerr << "    mouse window NDC coords " << normx << ' ' << normy << '\n';
    std::cerr << "    center image coords " << m_centerx << ' ' << m_centery << "\n";
    std::cerr << "    left,top = " << left << ' ' << top << "\n";
    std::cerr << "    right,bottom = " << right << ' ' << bottom << "\n";
    std::cerr << "    mouse image coords " << imgx << ' ' << imgy << "\n";
    std::cerr << "    mouse pixel image coords " << x << ' ' << y << "\n";
#endif
}



inline void
IvGL::gl_use_program (int program)
{
    if (m_shaders_using_extensions) 
        glUseProgramObjectARB (program);
    else
        glUseProgram (program);
}



inline GLint
IvGL::gl_get_uniform_location (const char *uniform)
{
    if (m_shaders_using_extensions)
        return glGetUniformLocationARB (m_shader_program, uniform);
    else
        return glGetUniformLocation (m_shader_program, uniform);
}



inline void
IvGL::gl_uniform (GLint location, float value)
{
    if (m_shaders_using_extensions)
        glUniform1fARB (location, value);
    else
        glUniform1f (location, value);
}



inline void
IvGL::gl_uniform (GLint location, int value)
{
    if (m_shaders_using_extensions)
        glUniform1iARB (location, value);
    else
        glUniform1i (location, value);
}



void
IvGL::check_gl_extensions (void)
{
#ifndef FORCE_OPENGL_1
    m_use_shaders = glewIsSupported("GL_VERSION_2_0");

    if (!m_use_shaders && glewIsSupported("GL_ARB_shader_objects "
                                          "GL_ARB_vertex_shader "
                                          "GL_ARB_fragment_shader")) {
        m_use_shaders = true;
        m_shaders_using_extensions = true;
    }

    m_use_srgb = glewIsSupported("GL_VERSION_2_1") ||
                 glewIsSupported("GL_EXT_texture_sRGB");

    m_use_halffloat = glewIsSupported("GL_VERSION_3_0") ||
                      glewIsSupported("GL_ARB_half_float_pixel") ||
                      glewIsSupported("GL_NV_half_float_pixel");

    m_use_float = glewIsSupported("GL_VERSION_3_0") ||
                  glewIsSupported("GL_ARB_texture_float") ||
                  glewIsSupported("GL_ATI_texture_float");

    m_use_npot_texture = glewIsSupported("GL_VERSION_2_0") ||
                         glewIsSupported("GL_ARB_texture_non_power_of_two");
#else
    std::cerr << "Not checking GL extensions\n";
#endif

    m_max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_max_texture_size);

#ifdef DEBUG
    // Report back...
    std::cerr << "OpenGL Shading Language supported: " << m_use_shaders << "\n";
    if (m_shaders_using_extensions) {
        std::cerr << "\t(with extensions)\n";
    }
    std::cerr << "OpenGL sRGB color space textures supported: " << m_use_srgb << "\n";
    std::cerr << "OpenGL half-float pixels supported: " << m_use_halffloat << "\n";
    std::cerr << "OpenGL float texture storage supported: " << m_use_float << "\n";
    std::cerr << "OpenGL non power of two textures suported: " << m_use_npot_texture << "\n";
    std::cerr << "OpenGL max texture dimension: " << m_max_texture_size << "\n";
#endif
}
