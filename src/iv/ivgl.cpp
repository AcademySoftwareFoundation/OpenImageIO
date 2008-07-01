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

#include <half.h>
#include <ImathFun.h>
#include <QGLFormat>

#include "imageviewer.h"



#define GLERRPRINT(msg)                                           \
    for (GLenum err = glGetError();  err != GL_NO_ERROR;  err = glGetError()) \
        std::cerr << "GL error " << msg << " " << (int)err << "\n";      \




IvGL::IvGL (QWidget *parent, ImageViewer &viewer)
    : QGLWidget(parent), m_viewer(viewer), m_pixelview(false),
      m_shaders_created(false), m_tex_created(false),
      m_centerx(0.5), m_centery(0.5)
{
    QGLFormat format;
    format.setRedBufferSize (32);
    format.setGreenBufferSize (32);
    format.setBlueBufferSize (32);
    format.setAlphaBufferSize (32);
    format.setDepth (true);
    setFormat (format);
}



IvGL::~IvGL ()
{
}



void
IvGL::initializeGL ()
{
    std::cerr << "initializeGL\n";
    if (m_pixelview)
    glClearColor (0.5, 0.05, 0.05, 1.0);
    else
    glClearColor (0.05, 0.05, 0.05, 1.0);
    glShadeModel (GL_FLAT);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
//    glEnable (GL_TEXTURE_2D);

    create_textures ();
    create_shaders ();
}



void
IvGL::create_textures (void)
{
    if (m_tex_created)
        return;

    glGenTextures (1, &m_texid);
    m_texid = 0;
    std::cerr << "m_texid = " << m_texid << ", texture0=" << GL_TEXTURE0 << "\n";
//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, m_texid);
    half pix[4] = {.25, .25, 1, 1};
#if 1
    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  4 /*internal format - color components */,
                  1 /*width*/, 1 /*height*/, 0 /*border width*/,
                  GL_RGBA /*type - GL_RGB, GL_RGBA, GL_LUMINANCE */,
                  GL_HALF_FLOAT_ARB /*format - GL_FLOAT */,
                  (const GLvoid *)pix /*data*/);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
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
        "uniform int width;\n"
        "uniform int height;\n"
        "void main ()\n"
        "{\n"
        "    vec2 st = vTexCoord;\n"
        "    if (pixelview != 0) {\n"
        "        vec2 wh = vec2(width,height);\n"
        "        st = floor(st * wh + vec2(0.5,0.5)) / wh;\n"
        "    }\n"
        "    vec4 C = texture2D (imgtex, st);\n"
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
        "        float lum = dot (C.xyz, vec3(0.3086, 0.6094, 0.0820));\n"
        "        C.xyz = vec3 (lum, lum, lum);\n"
        "    }\n"
        "    C.xyz *= gain;\n"
        "    float invgamma = 1.0/gamma;\n"
        "    C.xyz = pow (C.xyz, vec3 (invgamma, invgamma, invgamma));\n"
//        "    if (pixelview != 0)\n"
//        "        C = vec4(vTexCoord.x,vTexCoord.y,0,1);\n"
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

    useshader ();
    GLint loc;
    loc = glGetUniformLocation (m_shader_program, "imgtex");
    GLERRPRINT ("use tex 1");
    glUniform1i (loc, m_texid /*texture sampler number*/);
    GLERRPRINT ("use tex 2");
}



void
IvGL::resizeGL (int w, int h)
{
    if (m_pixelview)
    std::cerr << "resizeGL " << w << ' ' << h << "\n";
    GLERRPRINT ("resizeGL entry");
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
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (! m_viewer.cur())
        return;
 
    IvImage *img = m_viewer.cur();
    const ImageIOFormatSpec &spec (img->spec());
    float z = m_pixelview ? 1 : m_viewer.zoom();

    if (! m_pixelview) {
        if (z*spec.width <= width())  // FIXME - ivgl window?
            m_centerx = 0.5;
        if (z*spec.height <= height())
            m_centery = 0.5;
    }


    glPushMatrix ();
    glLoadIdentity ();
//    if (m_pixelview)
//    std::cerr << "paintGL, center " << m_centerx << ' ' << m_centery << '\n';
    glScalef (z, z, 1);
    glTranslatef (0, 0, -5.0);
    glScalef (spec.width, spec.height, 1);
    glTranslatef (-(m_centerx-0.5f), (m_centery-0.5f), 0.0f);

    useshader ();

//    if (m_pixelview) return;
#if 1
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
#endif
    glPopMatrix ();

    if (m_viewer.pixelviewOn()) {
        const int ncloseuppixels = 9;   // How many pixels to show in each dir
        const int closeuppixelzoom = 24;
        const int closeupsize = ncloseuppixels * closeuppixelzoom;
        int xp, yp;
        m_viewer.glwin->get_focus_pixel (xp, yp);
        float x = xp / (z * spec.width);
        float y = yp / (z * spec.height);

        glPushMatrix ();
        glLoadIdentity ();
        glTranslatef (0, 0, -1);
#if 0
        // Display closeup in upper left corner
        glTranslatef (-spec.width*0.5 + closeupsize*0.5f + 5,
                      spec.height*0.5 - closeupsize*0.5f - 5, 0);
#else
        // Display closeup overtop mouse
        glTranslatef (z * (xp - spec.width/2), z * (spec.height/2 - yp), 0);
#endif
        glScalef (spec.width, spec.height, 1);
        glScalef ((float)closeupsize/spec.width, (float)closeupsize/spec.height, 1);

        useshader (true);

        float xsize = 0.5 * (float)ncloseuppixels / spec.width;
        float ysize = 0.5 * (float)ncloseuppixels / spec.height;
        glBegin (GL_QUAD_STRIP);
        glTexCoord2f (x - xsize, y - ysize);
        glVertex3f (-0.5f, 0.5f, 1.0f);
        glTexCoord2f (x + xsize, y - ysize);
        glVertex3f (0.5f, 0.5f, 1.0f);
        glTexCoord2f (x - xsize, y + ysize);
        glVertex3f (-0.5f, -0.5f, 1.0f);
        glTexCoord2f (x + xsize, y + ysize);
        glVertex3f (0.5f, -0.5f, 1.0f);

        setFont (QFont("Times", 24));
        qglColor (QColor (1.0, 1.0, 1.0));
        renderText (0.0, 0.0, 0.0, QString("Test"));
        glEnd ();

        glPopMatrix ();
    }

//////////////

    swapBuffers ();
}



void
IvGL::useshader (bool pixelview)
{
    IvImage *img = m_viewer.cur();
    if (! img)
        return;
    const ImageIOFormatSpec &spec (img->spec());

//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
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
    if (m_pixelview) return;

    const ImageIOFormatSpec &spec (img->spec());
//    glActiveTexture (GL_TEXTURE0);
//    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, m_texid);
    std::cerr << "update " << (m_pixelview ? "pv " : "") <<" : bind " << m_texid << "\n";

    GLenum glformat = GL_RGB;
    if (spec.nchannels == 1)
        glformat = GL_LUMINANCE;
    else if (spec.nchannels == 4)
        glformat = GL_RGBA;

    GLenum gltype = GL_UNSIGNED_BYTE;
    switch (spec.format) {
    case PT_FLOAT  : gltype = GL_FLOAT;          break;
    case PT_HALF   : gltype = GL_HALF_FLOAT_ARB; break;
    case PT_INT8   : gltype = GL_BYTE;           break;
    case PT_UINT8  : gltype = GL_UNSIGNED_BYTE;  break;
    case PT_INT16  : gltype = GL_SHORT;          break;
    case PT_UINT16 : gltype = GL_UNSIGNED_SHORT; break;
    case PT_INT    : gltype = GL_INT;            break;
    case PT_UINT   : gltype = GL_UNSIGNED_INT;   break;
    default:
        gltype = GL_UNSIGNED_BYTE;  // punt
        break;
    }

    glTexImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                  spec.nchannels /*internal format - color components */,
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
            glTexSubImage2D (GL_TEXTURE_2D, 0 /*mip level*/,
                             0, y, spec.width, 1,
                             glformat, gltype, 
                             (const GLvoid *)img->scanline(y) /*data*/);
        }
    }
}



void
IvGL::center (float x, float y)
{
    m_centerx = x;
    m_centery = y;
    trigger_redraw ();
}



IvGLMainview::IvGLMainview (QWidget *parent, ImageViewer &viewer)
    : IvGL(parent, viewer), m_dragging(false)
{
    setMouseTracking (true);
}



void
IvGLMainview::zoom (float z)
{
    IvImage *img = m_viewer.cur();
    if (img) {
        const ImageIOFormatSpec &spec (img->spec());
        clamp_view_to_window ();
        repaint (0, 0, spec.width, spec.height);     // Update the texture
    } else {
        repaint (0, 0, width(), height());
    }
}



void
IvGLMainview::center (float x, float y)
{
    m_centerx = x;
    m_centery = y;
    clamp_view_to_window ();
    trigger_redraw ();
}



void
IvGLMainview::pan (float dx, float dy)
{
    center (m_centerx + dx, m_centery + dy);
}



void
IvGLMainview::remember_mouse (const QPoint &pos)
{
    m_mousex = pos.x();
    m_mousey = pos.y();
}



void
IvGLMainview::clamp_view_to_window ()
{
    int w = width(), h = height();
    float z = m_viewer.zoom ();
    float zoomedwidth  = z * m_viewer.curspec()->width;
    float zoomedheight = z * m_viewer.curspec()->height;
    float left    = m_centerx - 0.5 * ((float)w / zoomedwidth);
    float top     = m_centery - 0.5 * ((float)h / zoomedheight);
    float right   = m_centerx + 0.5 * ((float)w / zoomedwidth);
    float bottom  = m_centery + 0.5 * ((float)h / zoomedheight);
#if 0
    std::cerr << "Window size is " << w << " x " << h << "\n";
    std::cerr << "Center (normalized coords) is " << m_centerx << ", " << m_centery << "\n";
    std::cerr << "Top left (normalized coords) is " << left << ", " << top << "\n";
    std::cerr << "Bottom right (normalized coords) is " << right << ", " << bottom << "\n";
#endif

    // Don't let us scroll off the edges
    if (zoomedwidth >= w) {
        if (left < 0) {
            right -= left;
            m_centerx -= left;
            left = 0;
        } else if (right > 1) {
            m_centerx -= (right-1);
            left -= (right-1);
            right = 1;
        }
    } else {
        m_centerx = 0.5;
    }

    if (zoomedheight >= h) {
        if (top < 0) {
            bottom -= top;
            m_centery -= top;
            top = 0;
        } else if (bottom > 1) {
            m_centery -= (bottom-1);
            top -= (bottom-1);
            bottom = 1;
        }
    } else {
        m_centery = 0.5;
    }
}



void
IvGLMainview::mousePressEvent (QMouseEvent *event)
{
    remember_mouse (event->pos());
    m_drag_button = event->button();
    switch (event->button()) {
    case Qt::LeftButton :
        m_viewer.zoomIn();
        return;
    case Qt::RightButton :
        m_viewer.zoomOut();
        return;
    case Qt::MidButton :
        m_dragging = true;
        break;
    }
    parent_t::mousePressEvent (event);
}



void
IvGLMainview::mouseReleaseEvent (QMouseEvent *event)
{
    remember_mouse (event->pos());
    m_drag_button = Qt::NoButton;
    switch (event->button()) {
    case Qt::MidButton :
        m_dragging = false;
        break;
    }
    parent_t::mouseReleaseEvent (event);
}



void
IvGLMainview::mouseMoveEvent (QMouseEvent *event)
{
    QPoint pos = event->pos();
    // FIXME - there's probably a better Qt way than tracking the button
    // myself.
    switch (m_drag_button /*event->button()*/) {
    case Qt::MidButton : {
        float z = m_viewer.zoom ();
        float dx = (pos.x() - m_mousex) / (z * m_viewer.curspec()->width);
        float dy = (pos.y() - m_mousey) / (z * m_viewer.curspec()->height);
        pan (-dx, -dy);
        break;
        }
    }
    remember_mouse (pos);
    if (m_viewer.pixelviewWindow) {
        float z = m_viewer.zoom ();
        float x = pos.x() / (z * m_viewer.curspec()->width);
        float y = pos.y() / (z * m_viewer.curspec()->height);
        m_viewer.pixelviewWindow->center (x, y);
        m_viewer.pixelviewWindow->update (m_viewer.cur());
    }
    if (m_viewer.pixelviewOn())
        trigger_redraw ();
    parent_t::mouseMoveEvent (event);
}



void
IvGLMainview::wheelEvent (QWheelEvent *event)
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
IvGLMainview::get_focus_pixel (int &x, int &y)
{
    int w = width(), h = height();
    float z = m_viewer.zoom ();
    float zoomedwidth  = z * m_viewer.curspec()->width;
    float zoomedheight = z * m_viewer.curspec()->height;
    float left    = m_centerx - 0.5 * ((float)w / zoomedwidth);
    float top     = m_centery - 0.5 * ((float)h / zoomedheight);
    float right   = m_centerx + 0.5 * ((float)w / zoomedwidth);
    float bottom  = m_centery + 0.5 * ((float)h / zoomedheight);

    float normx = (float)(m_mousex + 0.5f) / w;
    float normy = (float)(m_mousey + 0.5f) / h;
    float imgx = Imath::lerp (left, right, normx);
    float imgy = Imath::lerp (top, bottom, normy);
    int pixx = imgx * m_viewer.curspec()->width;
    int pixy = imgy * m_viewer.curspec()->height;
    x = pixx;
    y = pixy;
#if 0
    std::cerr << "get_focus_pixel\n";
    std::cerr << "    mouse window pixel coords " << m_mousex << ' ' << m_mousey << "\n";
    std::cerr << "    mouse window NDC coords " << normx << ' ' << normy << '\n';
    std::cerr << "    center image coords " << m_centerx << ' ' << m_centery << "\n";
    std::cerr << "    left,top = " << left << ' ' << top << "\n";
    std::cerr << "    right,bottom = " << right << ' ' << bottom << "\n";
    std::cerr << "    mouse image coords " << imgx << ' ' << imgy << "\n";
    std::cerr << "    mouse pixel image coords " << pixx << ' ' << pixy << "\n";
#endif
}
