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

#include <half.h>
#include <ImathFun.h>
#include <QGLFormat>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/compare.hpp>


#include "imageviewer.h"
#include "strutil.h"

#ifdef WIN32
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>
#include <GL/wglext.h>
#endif

#define USE_SHADERS 1
#define USE_SRGB 1

#if ! defined (GL_SRGB_EXT) && defined (GL_SRGB)
#  define GL_SRGB_EXT GL_SRGB
#endif
#if ! defined (GL_SRGB8_EXT) && defined (GL_SRGB8)
#  define GL_SRGB8_EXT GL_SRGB8
#endif
#if ! defined (GL_SRGB_ALPHA_EXT) && defined (GL_SRGB_ALPHA)
#  define GL_SRGB_ALPHA_EXT GL_SRGB_ALPHA
#endif
#if ! defined (GL_SRGB8_ALPHA8_EXT) && defined (GL_SRGB8_ALPHA8)
#  define GL_SRGB8_ALPHA8_EXT GL_SRGB8_ALPHA8
#endif



#define GLERRPRINT(msg)                                           \
    for (GLenum err = glGetError();  err != GL_NO_ERROR;  err = glGetError()) \
        std::cerr << "GL error " << msg << " " << (int)err << "\n";      \




IvGL::IvGL (QWidget *parent, ImageViewer &viewer)
    : QGLWidget(parent), m_viewer(viewer), 
      m_shaders_created(false), m_tex_created(false),
      m_zoom(1.0), m_centerx(0), m_centery(0), m_dragging(false),
      m_target_texture(GL_TEXTURE_2D), m_unnormalized_coords(false)
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
    glClearColor (0.05, 0.05, 0.05, 1.0);
    glShadeModel (GL_FLAT);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glEnable (GL_ALPHA_TEST);
    glEnable (GL_BLEND);
//    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
//    glEnable (GL_TEXTURE_2D);

    // here we check what OpenGL extensions are available, and take action
    // if needed
    check_gl_extensions ();

    create_textures ();
#ifdef USE_SHADERS
    create_shaders ();
#endif
}



void
IvGL::create_textures (void)
{
    if (m_tex_created)
        return;

    glGenTextures (1, &m_texid);
    m_texid = 0;
//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
    glBindTexture (m_target_texture, m_texid);
    half pix[4] = {.25, .25, 1, 1};
#if 1
    glTexImage2D (m_target_texture, 0 /*mip level*/,
                  4 /*internal format - color components */,
                  1 /*width*/, 1 /*height*/, 0 /*border width*/,
                  GL_RGBA /*type - GL_RGB, GL_RGBA, GL_LUMINANCE */,
                  GL_HALF_FLOAT_ARB /*format - GL_FLOAT */,
                  (const GLvoid *)pix /*data*/);
    glTexParameteri (m_target_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (m_target_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (m_target_texture, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (m_target_texture, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif

    GLERRPRINT ("bind tex 1");
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
    if (m_shaders_created)
        return;

    m_shader_program = glCreateProgram ();

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
    if (! status) {
        std::cerr << "vertex shader compile status: " << status << "\n";
    }
    glAttachShader (m_shader_program, m_vertex_shader);
    GLERRPRINT ("vertex shader");

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
    m_fragment_shader = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (m_fragment_shader, 1, &fragment_source, NULL);
    glCompileShader (m_fragment_shader);
    glGetShaderiv (m_fragment_shader, GL_COMPILE_STATUS, &status);
    if (! status) {
        std::cerr << "fragment shader compile status: " << status << "\n";
        char buf[10000];
        buf[0] = 0;
        GLsizei len;
        glGetShaderInfoLog (m_fragment_shader, sizeof(buf), &len, buf);
        std::cerr << "compile log:\n" << buf << "---\n";
    }
    glAttachShader (m_shader_program, m_fragment_shader);
    GLERRPRINT ("fragment shader");

    glLinkProgram (m_shader_program);
    GLERRPRINT ("link");
    GLint linked, attached_shaders;
    glGetProgramiv (m_shader_program, GL_LINK_STATUS, &linked);
    if (! linked)
        std::cerr << "NOT LINKED\n";
    GLERRPRINT ("check link");
    glGetProgramiv (m_shader_program, GL_ATTACHED_SHADERS, &attached_shaders);
    if (attached_shaders != 2)
        std::cerr << "attached shaders: " << (int)attached_shaders << "\n";

    m_shaders_created = true;

#if 0
//    useshader ();

    GLint loc;
    loc = glGetUniformLocation (m_shader_program, "imgtex");
    GLERRPRINT ("use tex 1");
    glUniform1i (loc, m_texid /*texture sampler number*/);
    GLERRPRINT ("use tex 2");
#endif
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

#if 0
    // If the on-screen application window is larger than the full image
    // size, always center the image.
    float midx = img->oriented_full_x() + 0.5 * img->oriented_full_width();
    float midy = img->oriented_full_y() + 0.5 * img->oriented_full_height();
    if (z*img->oriented_full_width() <= width())
        m_centerx = midx;
    if (z*img->oriented_full_height() <= height())
        m_centery = midy;
#endif

    glPushAttrib (GL_ALL_ATTRIB_BITS);
    glPushMatrix ();
    glLoadIdentity ();
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
    float smin = 0, smax = 1, tmin = 0, tmax = 1;
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

    // used with GL_TEXTURE_RECTANGLE_ARB textures
    if (m_unnormalized_coords) {
        smax = xmax;
        tmax = ymax;
    }
    gl_rect (xmin, ymin, xmax, ymax, 0, smin, tmin, smax, tmax, rotate);

    glPopMatrix ();
    glPopAttrib ();

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
    glLoadIdentity ();
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
    glPushAttrib (GL_ALL_ATTRIB_BITS);
    useshader (true);
    float xtexsize = 0.5 * (float)ncloseuppixels / img->oriented_width();
    float ytexsize = 0.5 * (float)ncloseuppixels / img->oriented_height();
    // Make (x,y) be the image space NDC coords of the mouse.
    float x = (xp+0.5f) / (/* ? z * */ img->oriented_width());
    float y = (yp+0.5f) / (/* ? z * */ img->oriented_height());
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
    int xwin, ywin;
    get_focus_image_pixel (xwin, ywin);
    const int yspacing = 18;

    glPushAttrib (GL_ALL_ATTRIB_BITS);
    glUseProgram (0);  // No shader
    float extraspace = yspacing * (1 + spec.nchannels) + 4;
    glColor4f (0.1, 0.1, 0.1, 0.5);
    gl_rect (-0.5f*closeupsize-2, 0.5f*closeupsize+2,
             0.5f*closeupsize+2, -0.5f*closeupsize - extraspace, -0.1);

    // Now we print text giving the mouse coordinates and the numerical
    // values of the pixel that the mouse is over.
    QFont font;
    font.setFixedPitch (true);
//        std::cerr << "pixel size " << font.pixelSize() << "\n";
//    font.setPixelSize (16);
//    font.setFixedPitch (20);
//    bgfont.setPixelSize (20);
    if (xp >= 0 && xp < img->oriented_width() && yp >= 0 && yp < img->oriented_height()) {
        float *fpixel = (float *) alloca (spec.nchannels*sizeof(float));
        int textx = - closeupsize/2 + 4;
        int texty = - closeupsize/2 - yspacing;
        std::string s = Strutil::format ("(%d, %d)", xp+spec.x, yp+spec.y);
        shadowed_text (textx, texty, 0.0f, s, font);
        texty -= yspacing;
        img->getpixel (xp+spec.x, yp+spec.y, fpixel);
        const void *p = img->pixeladdr (xp+spec.x, yp+spec.y);
        for (int i = 0;  i < spec.nchannels;  ++i) {
            switch (spec.format.basetype) {
            case TypeDesc::UINT8 :
                s = Strutil::format ("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(),
                                     (int)((unsigned char *)p)[i], fpixel[i]);
                break;
            case TypeDesc::UINT16 :
                s = Strutil::format ("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(),
                                     (int)((unsigned short *)p)[i], fpixel[i]);
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
#if (USE_SHADERS == 0)
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    return;
#endif

    IvImage *img = m_viewer.cur();
    if (! img)
        return;
    const ImageSpec &spec (img->spec());

    glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, m_texid);

    GLERRPRINT ("before use program");
    glUseProgram (m_shader_program);
    GLERRPRINT ("use program");

    GLint loc;
#if 0
    loc = glGetUniformLocation (m_shader_program, "imgtex");
    GLERRPRINT ("set param 1");
    glUniform1i (loc, m_texid);
    GLERRPRINT ("set param 2");
#endif
    loc = glGetUniformLocation (m_shader_program, "gain");
    GLERRPRINT ("set param 3");
//    std::cerr << "loc for gain is " << (int)loc << '\n';
    float gain = powf (2.0, img->exposure());
    glUniform1f (loc, gain);
    GLERRPRINT ("set param 4");
    loc = glGetUniformLocation (m_shader_program, "gamma");
//    std::cerr << "loc for gamma is " << (int)loc << '\n';
    glUniform1f (loc, img->gamma());
    GLERRPRINT ("set param 5");
    loc = glGetUniformLocation (m_shader_program, "channelview");
    glUniform1i (loc, m_viewer.current_channel());
    GLERRPRINT ("set param 5");

    loc = glGetUniformLocation (m_shader_program, "imgchannels");
    glUniform1i (loc, spec.nchannels);
    GLERRPRINT ("set param 6");

    loc = glGetUniformLocation (m_shader_program, "pixelview");
    glUniform1i (loc, pixelview);
    GLERRPRINT ("set param 7");

    loc = glGetUniformLocation (m_shader_program, "linearinterp");
    glUniform1i (loc, m_viewer.linearInterpolation());
    GLERRPRINT ("set param 7");

    loc = glGetUniformLocation (m_shader_program, "width");
    glUniform1i (loc, spec.width);
    GLERRPRINT ("set param 8");
    loc = glGetUniformLocation (m_shader_program, "height");
    glUniform1i (loc, spec.height);
    GLERRPRINT ("set param 9");
}



void
IvGL::update (IvImage *img)
{
    if (! img)
        return;

//    std::cerr << "update image\n";

    const ImageSpec &spec (img->spec());
//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
    glBindTexture (m_target_texture, m_texid);

    bool srgb = (USE_SRGB && spec.linearity == ImageSpec::sRGB);
    GLenum glformat = GL_RGB;
    GLint glinternalformat = spec.nchannels;
    if (spec.nchannels == 1) {
        glformat = GL_LUMINANCE32F_ARB;
    } else if (spec.nchannels == 3) {
        glformat = GL_RGB;
        if (spec.format.basetype == TypeDesc::FLOAT)
            glinternalformat = srgb ? GL_SRGB_EXT : GL_RGB32F_ARB;
        else if (spec.format.basetype == TypeDesc::UINT8)
            glinternalformat = srgb ? GL_SRGB8_EXT : GL_RGB;
        else if (spec.format.basetype == TypeDesc::HALF)
            glinternalformat = srgb ? GL_SRGB_EXT : GL_RGB16F_ARB;
    } else if (spec.nchannels == 4) {
        glformat = GL_RGBA;
        if (spec.format.basetype == TypeDesc::FLOAT)
            glinternalformat = srgb ? GL_SRGB_ALPHA_EXT : GL_RGBA32F_ARB;
        else if (spec.format.basetype == TypeDesc::UINT8)
            glinternalformat = srgb ? GL_SRGB8_ALPHA8_EXT : GL_RGBA;
        else if (spec.format.basetype == TypeDesc::HALF)
            glinternalformat = srgb ? GL_SRGB_ALPHA_EXT : GL_RGBA16F_ARB;
    }

    GLenum gltype = GL_UNSIGNED_BYTE;
    switch (spec.format.basetype) {
    case TypeDesc::FLOAT  : gltype = GL_FLOAT;          break;
    case TypeDesc::HALF   : gltype = GL_HALF_FLOAT_ARB; break;
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

    glTexImage2D (m_target_texture, 0 /*mip level*/,
                  glinternalformat,
                  spec.width, spec.height,
                  0 /*border width*/,
                  glformat, gltype, 
                  (const GLvoid *)img->scanline(0) /*data*/);

    // Work around... bug? ... wherein odd-sized scanlines don't seem to
    // download the texture correctly, at least in OSX 10.5's OpenGL.
    // I found an effective workaround is to send each scanline separately.
    // Keep on the lookout for other conditions that trigger this problem,
    // maybe my assumption that it's about odd-length width is wrong.
    if (spec.width & 1) {
        for (int y = 0;  y < spec.height;  ++y) {
            glTexSubImage2D (m_target_texture, 0 /*mip level*/,
                             0, y, spec.width, 1,
                             glformat, gltype, 
                             (const GLvoid *)img->scanline(y) /*data*/);
        }
    }
//    std::cerr << "done\n\n";
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



void
IvGL::check_gl_extensions (void)
{
    // getting OpenGL version
    const GLubyte *version_string = glGetString(GL_VERSION);
    double gl_version = 0.0;
    gl_version = atof((const char*)version_string);

    // OpenGL 2.1 or more implies all the features we need are supported
    // (NPOT textures, GL_HALF_FLOAT, GL_FLOAT), great!
    if (gl_version >= 2.1)
        return;

    // turn on support for NPOT textures - if we can 
    if (gl_version >= 1.1) {
        std::string extensions ((const char*)glGetString (GL_EXTENSIONS));
        std::vector<std::string> allowed_extensions;
        boost::algorithm::split (allowed_extensions, extensions, boost::is_space());
        BOOST_FOREACH (const std::string &extension, allowed_extensions) {
            boost::is_equal test;

            // supported form OpenGL 1.4
            // NPOT textures are supported, normalized coords are supported
            if (test (extension, "GL_ARB_texture_non_power_of_two"))
                return;

            // supported from OpenGL 1.1
            // they are the same extensions: GL_ARB is approved by ARB
#ifdef GL_TEXTURE_RECTANGLE_ARB
            if (test (extension, "GL_ARB_texture_rectangle") ||
                test (extension, "GL_EXT_texture_rectangle")) {
                m_unnormalized_coords = true;
                m_target_texture = GL_TEXTURE_RECTANGLE_ARB;
                glEnable (m_target_texture);
                return;
            }
#endif

            // supported from OpenGL 1.2.1
#ifdef GL_TEXTURE_RECTANGLE_NV
            if (test (extension, "GL_NV_texture_rectangle")) {
                m_unnormalized_coords = true;
                m_target_texture = GL_TEXTURE_RECTANGLE_NV;
                glEnable (m_target_texture);
                return;
            }
#endif
        }
    }
}
