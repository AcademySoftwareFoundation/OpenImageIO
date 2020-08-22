// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include "ivgl.h"
#include "imageviewer.h"

#include <iostream>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <QComboBox>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>

#include "ivutils.h"
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>


static const char*
gl_err_to_string(GLenum err)
{  // Thanks, Dan Wexler, for this function
    switch (err) {
    case GL_NO_ERROR: return "No error";
    case GL_INVALID_ENUM: return "Invalid enum";
    case GL_INVALID_OPERATION: return "Invalid operation";
    case GL_INVALID_VALUE: return "Invalid value";
    case GL_OUT_OF_MEMORY: return "Out of memory";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "Invalid framebuffer operation";
    default: return "Unknown";
    }
}


#define GLERRPRINT(msg)                                                     \
    for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) \
        std::cerr << "GL error " << msg << " " << (int)err << " - "         \
                  << gl_err_to_string(err) << "\n";



IvGL::IvGL(QWidget* parent, ImageViewer& viewer)
    : QOpenGLWidget(parent)
    , m_viewer(viewer)
    , m_shaders_created(false)
    , m_tex_created(false)
    , m_zoom(1.0)
    , m_centerx(0)
    , m_centery(0)
    , m_dragging(false)
    , m_use_shaders(false)
    , m_use_halffloat(false)
    , m_use_float(false)
    , m_use_srgb(false)
    , m_texture_width(1)
    , m_texture_height(1)
    , m_last_pbo_used(0)
    , m_current_image(NULL)
    , m_pixelview_left_corner(true)
    , m_last_texbuf_used(0)
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
    m_mouse_activation = false;
    this->setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}



IvGL::~IvGL() {}



void
IvGL::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    // glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // Make sure initial matrix is identity (returning to this stack level loads
    // back this matrix).
    glLoadIdentity();

#if 1
    // Compensate for high res displays with device pixel ratio scaling
    float dpr = m_viewer.devicePixelRatio();
    glScalef(dpr, dpr, 1.0f);
#endif

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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // here we check what OpenGL extensions are available, and take action
    // if needed
    check_gl_extensions();

    create_textures();

    create_shaders();
}



void
IvGL::create_textures(void)
{
    if (m_tex_created)
        return;

    // FIXME: Determine this dynamically.
    const int total_texbufs = 4;
    GLuint textures[total_texbufs];

    glGenTextures(total_texbufs, textures);

    // Initialize texture objects
    for (unsigned int texture : textures) {
        m_texbufs.emplace_back();
        glBindTexture(GL_TEXTURE_2D, texture);
        GLERRPRINT("bind tex");
        glTexImage2D(GL_TEXTURE_2D, 0 /*mip level*/,
                     4 /*internal format - color components */, 1 /*width*/,
                     1 /*height*/, 0 /*border width*/,
                     GL_RGBA /*type - GL_RGB, GL_RGBA, GL_LUMINANCE */,
                     GL_FLOAT /*format - GL_FLOAT */, NULL /*data*/);
        GLERRPRINT("tex image 2d");
        // Initialize tex parameters.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        GLERRPRINT("After tex parameters");
        m_texbufs.back().tex_object = texture;
        m_texbufs.back().x          = 0;
        m_texbufs.back().y          = 0;
        m_texbufs.back().width      = 0;
        m_texbufs.back().height     = 0;
    }

    // Create another texture for the pixelview.
    glGenTextures(1, &m_pixelview_tex);
    glBindTexture(GL_TEXTURE_2D, m_pixelview_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, closeuptexsize, closeuptexsize, 0,
                 GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenBuffers(2, m_pbo_objects);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_objects[0]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_objects[1]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_tex_created = true;
}



void
IvGL::create_shaders(void)
{
    // clang-format off
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
        "uniform int startchannel;\n"
        "uniform int colormode;\n"
        // Remember, if imgchannels == 2, second channel would be channel 4 (a).
        "uniform int imgchannels;\n"
        "uniform int pixelview;\n"
        "uniform int linearinterp;\n"
        "uniform int width;\n"
        "uniform int height;\n"
        "vec4 rgba_mode (vec4 C)\n"
        "{\n"
        "    if (imgchannels <= 2) {\n"
        "        if (startchannel == 1)\n"
        "           return vec4(C.aaa, 1.0);\n"
        "        return C.rrra;\n"
        "    }\n"
        "    return C;\n"
        "}\n"
        "vec4 rgb_mode (vec4 C)\n"
        "{\n"
        "    if (imgchannels <= 2) {\n"
        "        if (startchannel == 1)\n"
        "           return vec4(C.aaa, 1.0);\n"
        "        return vec4 (C.rrr, 1.0);\n"
        "    }\n"
        "    float C2[4];\n"
        "    C2[0]=C.x; C2[1]=C.y; C2[2]=C.z; C2[3]=C.w;\n"
        "    return vec4 (C2[startchannel], C2[startchannel+1], C2[startchannel+2], 1.0);\n"
        "}\n"
        "vec4 singlechannel_mode (vec4 C)\n"
        "{\n"
        "    float C2[4];\n"
        "    C2[0]=C.x; C2[1]=C.y; C2[2]=C.z; C2[3]=C.w;\n"
        "    if (startchannel > imgchannels)\n"
        "        return vec4 (0.0,0.0,0.0,1.0);\n"
        "    return vec4 (C2[startchannel], C2[startchannel], C2[startchannel], 1.0);\n"
        "}\n"
        "vec4 luminance_mode (vec4 C)\n"
        "{\n"
        "    if (imgchannels <= 2)\n"
        "        return vec4 (C.rrr, C.a);\n"
        "    float lum = dot (C.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
        "    return vec4 (lum, lum, lum, C.a);\n"
        "}\n"
        "float heat_red(float x)\n"
        "{\n"
        "    return clamp (mix(0.0, 1.0, (x-0.35)/(0.66-0.35)), 0.0, 1.0) -\n"
        "           clamp (mix(0.0, 0.5, (x-0.89)/(1.0-0.89)), 0.0, 1.0);\n"
        "}\n"
        "float heat_green(float x)\n"
        "{\n"
        "    return clamp (mix(0.0, 1.0, (x-0.125)/(0.375-0.125)), 0.0, 1.0) -\n"
        "           clamp (mix(0.0, 1.0, (x-0.64)/(0.91-0.64)), 0.0, 1.0);\n"
        "}\n"
        "vec4 heatmap_mode (vec4 C)\n"
        "{\n"
        "    float C2[4];\n"
        "    C2[0]=C.x; C2[1]=C.y; C2[2]=C.z; C2[3]=C.w;\n"
        "    return vec4(heat_red(C2[startchannel]),\n"
        "                heat_green(C2[startchannel]),\n"
        "                heat_red(1.0-C2[startchannel]),\n"
        "                1.0);\n"
        "}\n"
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
        "    if (startchannel < 0)\n"
        "        C = vec4(0.0,0.0,0.0,1.0);\n"
        "    else if (colormode == 0)\n" // RGBA
        "        C = rgba_mode (C);\n"
        "    else if (colormode == 1)\n" // RGB (i.e., ignore alpha).
        "        C = rgb_mode (C);\n"
        "    else if (colormode == 2)\n" // Single channel.
        "        C = singlechannel_mode (C);\n"
        "    else if (colormode == 3)\n" // Luminance.
        "        C = luminance_mode (C);\n"
        "    else if (colormode == 4)\n" // Heatmap.
        "        C = heatmap_mode (C);\n"
        "    if (pixelview != 0)\n"
        "        C.a = 1.0;\n"
        "    C.xyz *= gain;\n"
        "    float invgamma = 1.0/gamma;\n"
        "    C.xyz = pow (C.xyz, vec3 (invgamma, invgamma, invgamma));\n"
        "    gl_FragColor = C;\n"
        "}\n";
    // clang-format on

    if (!m_use_shaders) {
        std::cerr << "Not using shaders!\n";
        return;
    }
    if (m_shaders_created)
        return;

    //initialize shader object handles for abort function
    m_shader_program  = 0;
    m_vertex_shader   = 0;
    m_fragment_shader = 0;

    // When using extensions to support shaders, we need to load the function
    // entry points (which is actually done by GLEW) and then call them. So
    // we have to get the functions through the right symbols otherwise
    // extension-based shaders won't work.
    m_shader_program = glCreateProgram();

    GLERRPRINT("create program");

    // This holds the compilation status
    GLint status;

    m_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(m_vertex_shader, 1, &vertex_source, NULL);
    glCompileShader(m_vertex_shader);
    glGetShaderiv(m_vertex_shader, GL_COMPILE_STATUS, &status);

    if (!status) {
        std::cerr << "vertex shader compile status: " << status << "\n";
        print_shader_log(std::cerr, m_vertex_shader);
        create_shaders_abort();
        return;
    }
    glAttachShader(m_shader_program, m_vertex_shader);
    GLERRPRINT("After attach vertex shader.");

    m_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_fragment_shader, 1, &fragment_source, NULL);
    glCompileShader(m_fragment_shader);
    glGetShaderiv(m_fragment_shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        std::cerr << "fragment shader compile status: " << status << "\n";
        print_shader_log(std::cerr, m_fragment_shader);
        create_shaders_abort();
        return;
    }
    glAttachShader(m_shader_program, m_fragment_shader);
    GLERRPRINT("After attach fragment shader");

    glLinkProgram(m_shader_program);
    GLERRPRINT("link");
    GLint linked;
    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::cerr << "NOT LINKED\n";
        char buf[10000];
        buf[0] = 0;
        GLsizei len;
        glGetProgramInfoLog(m_shader_program, sizeof(buf), &len, buf);
        std::cerr << "link log:\n" << buf << "---\n";
        create_shaders_abort();
        return;
    }

    m_shaders_created = true;
}



void
IvGL::create_shaders_abort(void)
{
    glUseProgram(0);
    if (m_shader_program)
        glDeleteProgram(m_shader_program);
    if (m_vertex_shader)
        glDeleteShader(m_vertex_shader);
    if (m_fragment_shader)
        glDeleteShader(m_fragment_shader);

    GLERRPRINT("After delete shaders");
    m_use_shaders = false;
}



void
IvGL::resizeGL(int w, int h)
{
    GLERRPRINT("resizeGL entry");
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-w / 2.0, w / 2.0, -h / 2.0, h / 2.0, 0, 10);
    // Main GL viewport is set up for orthographic view centered at
    // (0,0) and with width and height equal to the window dimensions IN
    // PIXEL UNITS.
    glMatrixMode(GL_MODELVIEW);

    clamp_view_to_window();
    GLERRPRINT("resizeGL exit");
}



static void
gl_rect(float xmin, float ymin, float xmax, float ymax, float z = 0,
        float smin = 0, float tmin = 0, float smax = 1, float tmax = 1,
        int rotate = 0)
{
    float tex[] = { smin, tmin, smax, tmin, smax, tmax, smin, tmax };
    glBegin(GL_POLYGON);
    glTexCoord2f(tex[(0 + 2 * rotate) & 7], tex[(1 + 2 * rotate) & 7]);
    glVertex3f(xmin, ymin, z);
    glTexCoord2f(tex[(2 + 2 * rotate) & 7], tex[(3 + 2 * rotate) & 7]);
    glVertex3f(xmax, ymin, z);
    glTexCoord2f(tex[(4 + 2 * rotate) & 7], tex[(5 + 2 * rotate) & 7]);
    glVertex3f(xmax, ymax, z);
    glTexCoord2f(tex[(6 + 2 * rotate) & 7], tex[(7 + 2 * rotate) & 7]);
    glVertex3f(xmin, ymax, z);
    glEnd();
}



static void
handle_orientation(int orientation, int width, int height, float& scale_x,
                   float& scale_y, float& rotate_z, float& point_x,
                   float& point_y, bool pixel = false)
{
    switch (orientation) {
    case 2:  // flipped horizontally
        scale_x = -1;
        point_x = width - point_x;
        if (pixel)
            // We want to access the pixel at (point_x,pointy), so we have to
            // substract 1 to get the right index.
            --point_x;
        break;
    case 3:  // bottom up, right to left (rotated 180).
        scale_x = -1;
        scale_y = -1;
        point_x = width - point_x;
        point_y = height - point_y;
        if (pixel) {
            --point_x;
            --point_y;
        }
        break;
    case 4:  // flipped vertically.
        scale_y = -1;
        point_y = height - point_y;
        if (pixel)
            --point_y;
        break;
    case 5:  // transposed (flip horizontal & rotated 90 ccw).
        scale_x  = -1;
        rotate_z = 90.0;
        std::swap(point_x, point_y);
        break;
    case 6:  // rotated 90 cw.
        rotate_z = -270.0;
        std::swap(point_x, point_y);
        point_y = height - point_y;
        if (pixel)
            --point_y;
        break;
    case 7:  // transverse, (flip horizontal & rotated 90 cw, r-to-l, b-to-t)
        scale_x  = -1;
        rotate_z = -90.0;
        std::swap(point_x, point_y);
        point_x = width - point_x;
        point_y = height - point_y;
        if (pixel) {
            --point_x;
            --point_y;
        }
        break;
    case 8:  // rotated 90 ccw.
        rotate_z = -90.0;
        std::swap(point_x, point_y);
        point_x = width - point_x;
        if (pixel)
            --point_x;
        break;
    case 1:  // horizontal
    case 0:  // unknown
    default: break;
    }
}



void
IvGL::paintGL()
{
#ifndef NDEBUG
    Timer paint_image_time;
    paint_image_time.start();
#endif
    //std::cerr << "paintGL " << m_viewer.current_image() << " with zoom " << m_zoom << "\n";
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    IvImage* img = m_current_image;
    if (!img || !img->image_valid())
        return;

    const ImageSpec& spec(img->spec());
    float z = m_zoom;

    glPushMatrix();
    glLoadIdentity();
    // Transform is now same as the main GL viewport -- window pixels as
    // units, with (0,0) at the center of the visible unit.
    glTranslatef(0, 0, -5);
    // Pushed away from the camera 5 units.
    glScalef(1, -1, 1);
    // Flip y, because OGL's y runs from bottom to top.
    glScalef(z, z, 1);
    // Scaled by zoom level.  So now xy units are image pixels as
    // displayed at the current zoom level, with the origin at the
    // center of the visible window.

    // Handle the orientation with OpenGL *before* translating our center.
    float scale_x      = 1;
    float scale_y      = 1;
    float rotate_z     = 0;
    float real_centerx = m_centerx;
    float real_centery = m_centery;
    handle_orientation(img->orientation(), spec.width, spec.height, scale_x,
                       scale_y, rotate_z, real_centerx, real_centery);

    glScalef(scale_x, scale_y, 1);
    glRotatef(rotate_z, 0, 0, 1);
    glTranslatef(-real_centerx, -real_centery, 0.0f);
    // Recentered so that the pixel space (m_centerx,m_centery) position is
    // at the center of the visible window.

    useshader(m_texture_width, m_texture_height);

    float smin = 0, smax = 1.0;
    float tmin = 0, tmax = 1.0;
    // Image pixels shown from the center to the edge of the window.
    int wincenterx = (int)ceil(width() / (2 * m_zoom));
    int wincentery = (int)ceil(height() / (2 * m_zoom));
    if (img->orientation() > 4) {
        std::swap(wincenterx, wincentery);
    }

    int xbegin = (int)floor(real_centerx) - wincenterx;
    xbegin     = std::max(spec.x, xbegin - (xbegin % m_texture_width));
    int ybegin = (int)floor(real_centery) - wincentery;
    ybegin     = std::max(spec.y, ybegin - (ybegin % m_texture_height));
    int xend   = (int)floor(real_centerx) + wincenterx;
    xend       = std::min(spec.x + spec.width,
                    xend + m_texture_width - (xend % m_texture_width));
    int yend   = (int)floor(real_centery) + wincentery;
    yend       = std::min(spec.y + spec.height,
                    yend + m_texture_height - (yend % m_texture_height));
    //std::cerr << "(" << xbegin << ',' << ybegin << ") - (" << xend << ',' << yend << ")\n";

    // Provide some feedback
    int total_tiles    = (int)(ceilf(float(xend - xbegin) / m_texture_width)
                            * ceilf(float(yend - ybegin) / m_texture_height));
    float tile_advance = 1.0f / total_tiles;
    float percent      = tile_advance;
    m_viewer.statusViewInfo->hide();
    m_viewer.statusProgress->show();

    // FIXME: change the code path so we can take full advantage of async DMA
    // when using PBO.
    for (int ystart = ybegin; ystart < yend; ystart += m_texture_height) {
        for (int xstart = xbegin; xstart < xend; xstart += m_texture_width) {
            int tile_width  = std::min(xend - xstart, m_texture_width);
            int tile_height = std::min(yend - ystart, m_texture_height);
            smax            = tile_width / float(m_texture_width);
            tmax            = tile_height / float(m_texture_height);

            //std::cerr << "xstart: " << xstart << ". ystart: " << ystart << "\n";
            //std::cerr << "tile_width: " << tile_width << ". tile_height: " << tile_height << "\n";

            // FIXME: This can get too slow. Some ideas: avoid sending the tex
            // images more than necessary, figure an optimum texture size, use
            // multiple texture objects.
            load_texture(xstart, ystart, tile_width, tile_height);
            gl_rect(xstart, ystart, xstart + tile_width, ystart + tile_height,
                    0, smin, tmin, smax, tmax);
            percent += tile_advance;
        }
    }

    glPopMatrix();

    if (m_viewer.pixelviewOn()) {
        paint_pixelview();
    }

    // Show the status info again.
    m_viewer.statusProgress->hide();
    m_viewer.statusViewInfo->show();
    unsetCursor();

#ifndef NDEBUG
    std::cerr << "paintGL elapsed time: " << paint_image_time() << " seconds\n";
#endif
}



void
IvGL::shadowed_text(float x, float y, float /*z*/, const std::string& s,
                    const QFont& font)
{
    /*
     * Paint on intermediate QImage, AA text on QOpenGLWidget based
     * QPaintDevice requires MSAA
     */
    QImage t(size(), QImage::Format_ARGB32_Premultiplied);
    t.fill(qRgba(0, 0, 0, 0));
    {
        QPainter painter(&t);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        painter.setFont(font);

        painter.setPen(QPen(Qt::white, 1.0));
        painter.drawText(QPointF(x, y), QString(s.c_str()));
    }
    QPainter painter(this);
    painter.drawImage(rect(), t);
}



static int
num_channels(int current_channel, int nchannels,
             ImageViewer::COLOR_MODE color_mode)
{
    switch (color_mode) {
    case ImageViewer::RGBA: return clamp(nchannels - current_channel, 0, 4);
    case ImageViewer::RGB:
    case ImageViewer::LUMINANCE:
        return clamp(nchannels - current_channel, 0, 3);
        break;
    case ImageViewer::SINGLE_CHANNEL:
    case ImageViewer::HEATMAP: return 1;
    default: return nchannels;
    }
}



void
IvGL::paint_pixelview()
{
    IvImage* img = m_current_image;
    const ImageSpec& spec(img->spec());

    // (xw,yw) are the window coordinates of the mouse.
    int xw, yw;
    get_focus_window_pixel(xw, yw);

    // (xp,yp) are the image-space [0..res-1] position of the mouse.
    int xp, yp;
    get_focus_image_pixel(xp, yp);

    glPushMatrix();
    glLoadIdentity();
    // Transform is now same as the main GL viewport -- window pixels as
    // units, with (0,0) at the center of the visible window.

    glTranslatef(0, 0, -1);
    // Pushed away from the camera 1 unit.  This makes the pixel view
    // elements closer to the camera than the main view.

    if (m_viewer.pixelviewFollowsMouse()) {
        // Display closeup overtop mouse -- translate the coordinate system
        // so that it is centered at the mouse position.
        glTranslatef(xw - width() / 2 + closeupsize / 2 + 4,
                     -yw + height() / 2 - closeupsize / 2 - 4, 0);
    } else {
        // Display closeup in corner -- translate the coordinate system so that
        // it is centered near the corner of the window.
        if (m_pixelview_left_corner) {
            glTranslatef(closeupsize * 0.5f + 5 - width() / 2,
                         -closeupsize * 0.5f - 5 + height() / 2, 0);
            // If the mouse cursor is over the pixelview closeup when it's on
            // the upper left, switch to the upper right
            if ((xw < closeupsize + 5) && yw < (closeupsize + 5))
                m_pixelview_left_corner = false;
        } else {
            glTranslatef(-closeupsize * 0.5f - 5 + width() / 2,
                         -closeupsize * 0.5f - 5 + height() / 2, 0);
            // If the mouse cursor is over the pixelview closeup when it's on
            // the upper right, switch to the upper left
            if (xw > (width() - closeupsize - 5) && yw < (closeupsize + 5))
                m_pixelview_left_corner = true;
        }
    }
    // In either case, the GL coordinate system is now scaled to window
    // pixel units, and centered on the middle of where the closeup
    // window is going to appear.  All other coordinates from here on
    // (in this procedure) should be relative to the closeup window center.

    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
    useshader(closeuptexsize, closeuptexsize, true);

    float scale_x  = 1.0f;
    float scale_y  = 1.0f;
    float rotate_z = 0.0f;
    float real_xp  = xp;
    float real_yp  = yp;
    handle_orientation(img->orientation(), spec.width, spec.height, scale_x,
                       scale_y, rotate_z, real_xp, real_yp, true);

    float smin = 0;
    float tmin = 0;
    float smax = 1.0f;
    float tmax = 1.0f;
    if (xp >= 0 && xp < img->oriented_width() && yp >= 0
        && yp < img->oriented_height()) {
        // Keep the view within ncloseuppixels pixels.
        int xpp = clamp<int>(real_xp, ncloseuppixels / 2,
                             spec.width - ncloseuppixels / 2 - 1);
        int ypp = clamp<int>(real_yp, ncloseuppixels / 2,
                             spec.height - ncloseuppixels / 2 - 1);
        // Calculate patch of the image to use for the pixelview.
        int xbegin = std::max(xpp - ncloseuppixels / 2, 0);
        int ybegin = std::max(ypp - ncloseuppixels / 2, 0);
        int xend   = std::min(xpp + ncloseuppixels / 2 + 1, spec.width);
        int yend   = std::min(ypp + ncloseuppixels / 2 + 1, spec.height);
        smin       = 0;
        tmin       = 0;
        smax       = float(xend - xbegin) / closeuptexsize;
        tmax       = float(yend - ybegin) / closeuptexsize;
        //std::cerr << "img (" << xbegin << "," << ybegin << ") - (" << xend << "," << yend << ")\n";
        //std::cerr << "tex (" << smin << "," << tmin << ") - (" << smax << "," << tmax << ")\n";
        //std::cerr << "center mouse (" << xp << "," << yp << "), real (" << real_xp << "," << real_yp << ")\n";

        int nchannels = img->nchannels();
        // For simplicity, we don't support more than 4 channels without shaders
        // (yet).
        if (m_use_shaders) {
            nchannels = num_channels(m_viewer.current_channel(), nchannels,
                                     m_viewer.current_color_mode());
        }

        void* zoombuffer = OIIO_ALLOCA(char, (xend - xbegin) * (yend - ybegin)
                                                 * nchannels
                                                 * spec.channel_bytes());
        if (!m_use_shaders) {
            img->get_pixels(ROI(spec.x + xbegin, spec.x + xend, spec.y + ybegin,
                                spec.y + yend),
                            spec.format, zoombuffer);
        } else {
            ROI roi(spec.x + xbegin, spec.x + xend, spec.y + ybegin,
                    spec.y + yend, 0, 1, m_viewer.current_channel(),
                    m_viewer.current_channel() + nchannels);
            img->get_pixels(roi, spec.format, zoombuffer);
        }

        GLenum glformat, gltype, glinternalformat;
        typespec_to_opengl(spec, nchannels, gltype, glformat, glinternalformat);
        // Use pixelview's own texture, and upload the corresponding image patch.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, m_pixelview_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, xend - xbegin, yend - ybegin,
                        glformat, gltype, zoombuffer);
        GLERRPRINT("After tsi2d");
    } else {
        smin = -1;
        smax = -1;
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.1f, 0.1f, 0.1f);
    }
    if (!m_use_shaders) {
        glDisable(GL_BLEND);
    }

    glPushMatrix();
    glScalef(1, -1, 1);  // Run y from top to bottom.
    glScalef(scale_x, scale_y, 1);
    glRotatef(rotate_z, 0, 0, 1);

    // This square is the closeup window itself
    gl_rect(-0.5f * closeupsize, -0.5f * closeupsize, 0.5f * closeupsize,
            0.5f * closeupsize, 0, smin, tmin, smax, tmax);
    glPopMatrix();
    glPopAttrib();

    // Draw a second window, slightly behind the closeup window, as a
    // backdrop.  It's partially transparent, having the effect of
    // darkening the main image view beneath the closeup window.  It
    // extends slightly out from the closeup window (making it more
    // clearly visible), and also all the way down to cover the area
    // where the text will be printed, so it is very readable.
    const int yspacing = 18;

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    if (m_use_shaders) {
        // Disable shaders for this.
        glUseProgram(0);
    }
    float extraspace = yspacing * (1 + spec.nchannels) + 4;
    glColor4f(0.1f, 0.1f, 0.1f, 0.5f);
    gl_rect(-0.5f * closeupsize - 2, 0.5f * closeupsize + 2,
            0.5f * closeupsize + 2, -0.5f * closeupsize - extraspace, -0.1f);

    if (1 /*xp >= 0 && xp < img->oriented_width() && yp >= 0 && yp < img->oriented_height()*/) {
        // Now we print text giving the mouse coordinates and the numerical
        // values of the pixel that the mouse is over.
        QFont font;
        font.setFixedPitch(true);
        float* fpixel = OIIO_ALLOCA(float, spec.nchannels);
        int textx, texty;
        if (m_viewer.pixelviewFollowsMouse()) {
            textx = xw + 8;
            texty = yw + closeupsize + yspacing;
        } else {
            if (m_pixelview_left_corner) {
                textx = 9;
                texty = closeupsize + yspacing;
            } else {
                textx = width() - closeupsize - 1;
                texty = closeupsize + yspacing;
            }
        }
        std::string s = Strutil::sprintf("(%d, %d)", (int)real_xp + spec.x,
                                         (int)real_yp + spec.y);
        shadowed_text(textx, texty, 0.0f, s, font);
        texty += yspacing;
        img->getpixel((int)real_xp + spec.x, (int)real_yp + spec.y, fpixel);
        for (int i = 0; i < spec.nchannels; ++i) {
            switch (spec.format.basetype) {
            case TypeDesc::UINT8: {
                ImageBuf::ConstIterator<unsigned char, unsigned char> p(
                    *img, (int)real_xp + spec.x, (int)real_yp + spec.y);
                s = Strutil::sprintf("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(), (int)(p[i]),
                                     fpixel[i]);
            } break;
            case TypeDesc::UINT16: {
                ImageBuf::ConstIterator<unsigned short, unsigned short> p(
                    *img, (int)real_xp + spec.x, (int)real_yp + spec.y);
                s = Strutil::sprintf("%s: %3d  (%5.3f)",
                                     spec.channelnames[i].c_str(), (int)(p[i]),
                                     fpixel[i]);
            } break;
            default:  // everything else, treat as float
                s = Strutil::sprintf("%s: %5.3f", spec.channelnames[i].c_str(),
                                     fpixel[i]);
            }
            shadowed_text(textx, texty, 0.0f, s, font);
            texty += yspacing;
        }
    }

    glPopAttrib();
    glPopMatrix();
}



void
IvGL::useshader(int tex_width, int tex_height, bool pixelview)
{
    IvImage* img = m_viewer.cur();
    if (!img)
        return;

    if (!m_use_shaders) {
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        for (auto&& tb : m_texbufs) {
            glBindTexture(GL_TEXTURE_2D, tb.tex_object);
            if (m_viewer.linearInterpolation()) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
            } else {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_NEAREST);
            }
        }
        return;
    }

    const ImageSpec& spec(img->spec());

    glUseProgram(m_shader_program);
    GLERRPRINT("After use program");

    GLint loc;

    loc = glGetUniformLocation(m_shader_program, "startchannel");
    if (m_viewer.current_channel() >= spec.nchannels) {
        glUniform1i(loc, -1);
        return;
    }
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(m_shader_program, "imgtex");
    // This is the texture unit, not the texture object
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(m_shader_program, "gain");

    float gain = powf(2.0, img->exposure());
    glUniform1f(loc, gain);

    loc = glGetUniformLocation(m_shader_program, "gamma");
    glUniform1f(loc, img->gamma());

    loc = glGetUniformLocation(m_shader_program, "colormode");
    glUniform1i(loc, m_viewer.current_color_mode());

    loc = glGetUniformLocation(m_shader_program, "imgchannels");
    glUniform1i(loc, spec.nchannels);

    loc = glGetUniformLocation(m_shader_program, "pixelview");
    glUniform1i(loc, pixelview);

    loc = glGetUniformLocation(m_shader_program, "linearinterp");
    glUniform1i(loc, m_viewer.linearInterpolation());

    loc = glGetUniformLocation(m_shader_program, "width");
    glUniform1i(loc, tex_width);

    loc = glGetUniformLocation(m_shader_program, "height");
    glUniform1i(loc, tex_height);
    GLERRPRINT("After setting uniforms");
}



void
IvGL::update()
{
    //std::cerr << "update image\n";

    IvImage* img = m_viewer.cur();
    if (!img) {
        m_current_image = NULL;
        return;
    }

    const ImageSpec& spec(img->spec());

    int nchannels = img->nchannels();
    // For simplicity, we don't support more than 4 channels without shaders
    // (yet).
    if (m_use_shaders) {
        nchannels = num_channels(m_viewer.current_channel(), nchannels,
                                 m_viewer.current_color_mode());
    }

    if (!nchannels)
        return;  // Don't bother, the shader will show blackness for us.

    GLenum gltype           = GL_UNSIGNED_BYTE;
    GLenum glformat         = GL_RGB;
    GLenum glinternalformat = GL_RGB;
    typespec_to_opengl(spec, nchannels, gltype, glformat, glinternalformat);

    m_texture_width  = clamp(ceil2(spec.width), 1, m_max_texture_size);
    m_texture_height = clamp(ceil2(spec.height), 1, m_max_texture_size);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    for (auto&& tb : m_texbufs) {
        tb.width  = 0;
        tb.height = 0;
        glBindTexture(GL_TEXTURE_2D, tb.tex_object);
        glTexImage2D(GL_TEXTURE_2D, 0 /*mip level*/, glinternalformat,
                     m_texture_width, m_texture_height, 0 /*border width*/,
                     glformat, gltype, NULL /*data*/);
        GLERRPRINT("Setting up texture");
    }

    // Set the right type for the texture used for pixelview.
    glBindTexture(GL_TEXTURE_2D, m_pixelview_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, closeuptexsize,
                 closeuptexsize, 0, glformat, gltype, NULL);
    GLERRPRINT("Setting up pixelview texture");

    // Resize the buffer at once, rather than create one each drawing.
    m_tex_buffer.resize(m_texture_width * m_texture_height * nchannels
                        * spec.channel_bytes());
    m_current_image = img;
}



void
IvGL::view(float xcenter, float ycenter, float zoom, bool redraw)
{
    m_centerx = xcenter;
    m_centery = ycenter;
    m_zoom    = zoom;

    if (redraw)
        parent_t::update();
}



void
IvGL::zoom(float newzoom, bool redraw)
{
    view(m_centerx, m_centery, newzoom, redraw);
}



void
IvGL::center(float x, float y, bool redraw)
{
    view(x, y, m_viewer.zoom(), redraw);
}



void
IvGL::pan(float dx, float dy)
{
    center(m_centerx + dx, m_centery + dy);
}



void
IvGL::remember_mouse(const QPoint& pos)
{
    m_mousex = pos.x();
    m_mousey = pos.y();
}



void
IvGL::clamp_view_to_window()
{
    IvImage* img = m_current_image;
    if (!img)
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

    int xmin = std::min(img->oriented_x(), img->oriented_full_x());
    int xmax = std::max(img->oriented_x() + img->oriented_width(),
                        img->oriented_full_x() + img->oriented_full_width());
    int ymin = std::min(img->oriented_y(), img->oriented_full_y());
    int ymax = std::max(img->oriented_y() + img->oriented_height(),
                        img->oriented_full_y() + img->oriented_full_height());

    // Don't let us scroll off the edges
    if (zoomedwidth >= w) {
        m_centerx = Imath::clamp(m_centerx, xmin + 0.5f * w / m_zoom,
                                 xmax - 0.5f * w / m_zoom);
    } else {
        m_centerx = img->oriented_full_x() + img->oriented_full_width() / 2;
    }

    if (zoomedheight >= h) {
        m_centery = Imath::clamp(m_centery, ymin + 0.5f * h / m_zoom,
                                 ymax - 0.5f * h / m_zoom);
    } else {
        m_centery = img->oriented_full_y() + img->oriented_full_height() / 2;
    }
}



void
IvGL::mousePressEvent(QMouseEvent* event)
{
    remember_mouse(event->pos());
    int mousemode = m_viewer.mouseModeComboBox->currentIndex();
    bool Alt      = (event->modifiers() & Qt::AltModifier);
    m_drag_button = event->button();
    if (!m_mouse_activation) {
        switch (event->button()) {
        case Qt::LeftButton:
            if (mousemode == ImageViewer::MouseModeZoom && !Alt)
                m_viewer.zoomIn();
            else
                m_dragging = true;
            return;
        case Qt::RightButton:
            if (mousemode == ImageViewer::MouseModeZoom && !Alt)
                m_viewer.zoomOut();
            else
                m_dragging = true;
            return;
        case Qt::MidButton:
            m_dragging = true;
            // FIXME: should this be return rather than break?
            break;
        default: break;
        }
    } else
        m_mouse_activation = false;
    parent_t::mousePressEvent(event);
}



void
IvGL::mouseReleaseEvent(QMouseEvent* event)
{
    remember_mouse(event->pos());
    m_drag_button = Qt::NoButton;
    m_dragging    = false;
    parent_t::mouseReleaseEvent(event);
}



void
IvGL::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();
    // FIXME - there's probably a better Qt way than tracking the button
    // myself.
    bool Alt      = (event->modifiers() & Qt::AltModifier);
    int mousemode = m_viewer.mouseModeComboBox->currentIndex();
    bool do_pan = false, do_zoom = false, do_wipe = false;
    bool do_select = false, do_annotate = false;
    switch (mousemode) {
    case ImageViewer::MouseModeZoom:
        if ((m_drag_button == Qt::MidButton)
            || (m_drag_button == Qt::LeftButton && Alt)) {
            do_pan = true;
        } else if (m_drag_button == Qt::RightButton && Alt) {
            do_zoom = true;
        }
        break;
    case ImageViewer::MouseModePan:
        if (m_drag_button != Qt::NoButton)
            do_pan = true;
        break;
    case ImageViewer::MouseModeWipe:
        if (m_drag_button != Qt::NoButton)
            do_wipe = true;
        break;
    case ImageViewer::MouseModeSelect:
        if (m_drag_button != Qt::NoButton)
            do_select = true;
        break;
    case ImageViewer::MouseModeAnnotate:
        if (m_drag_button != Qt::NoButton)
            do_annotate = true;
        break;
    }
    if (do_pan) {
        float dx = (pos.x() - m_mousex) / m_zoom;
        float dy = (pos.y() - m_mousey) / m_zoom;
        pan(-dx, -dy);
    } else if (do_zoom) {
        float dx = (pos.x() - m_mousex);
        float dy = (pos.y() - m_mousey);
        float z  = m_viewer.zoom() * (1.0 + 0.005 * (dx + dy));
        z        = Imath::clamp(z, 0.01f, 256.0f);
        m_viewer.zoom(z);
        m_viewer.fitImageToWindowAct->setChecked(false);
    } else if (do_wipe) {
        // FIXME -- unimplemented
    } else if (do_select) {
        // FIXME -- unimplemented
    } else if (do_annotate) {
        // FIXME -- unimplemented
    }
    remember_mouse(pos);
    if (m_viewer.pixelviewOn())
        parent_t::update();
    parent_t::mouseMoveEvent(event);
}


void
IvGL::wheelEvent(QWheelEvent* event)
{
    m_mouse_activation = false;
    QPoint angdelta    = event->angleDelta() / 8;  // div by 8 to get degrees
    if (abs(angdelta.y()) > abs(angdelta.x())      // predominantly vertical
        && abs(angdelta.y()) > 2) {                // suppress tiny motions
        float oldzoom = m_viewer.zoom();
        float newzoom = (angdelta.y() > 0) ? ceil2f(oldzoom) : floor2f(oldzoom);
        m_viewer.zoom(newzoom);
        event->accept();
    }
    // TODO: Update this to keep the zoom centered on the event .x, .y
}



void
IvGL::focusOutEvent(QFocusEvent*)
{
    m_mouse_activation = true;
}



void
IvGL::get_focus_window_pixel(int& x, int& y)
{
    x = m_mousex;
    y = m_mousey;
}



void
IvGL::get_focus_image_pixel(int& x, int& y)
{
    // w,h are the dimensions of the visible window, in pixels
    int w = width(), h = height();
    float z = m_zoom;
    // left,top,right,bottom are the borders of the visible window, in
    // pixel coordinates
    float left   = m_centerx - 0.5 * w / z;
    float top    = m_centery - 0.5 * h / z;
    float right  = m_centerx + 0.5 * w / z;
    float bottom = m_centery + 0.5 * h / z;
    // normx,normy are the position of the mouse, in normalized (i.e. [0..1])
    // visible window coordinates.
    float normx = (float)(m_mousex + 0.5f) / w;
    float normy = (float)(m_mousey + 0.5f) / h;
    // imgx,imgy are the position of the mouse, in pixel coordinates
    float imgx = Imath::lerp(left, right, normx);
    float imgy = Imath::lerp(top, bottom, normy);
    // So finally x,y are the coordinates of the image pixel (on [0,res-1])
    // underneath the mouse cursor.
    //FIXME: Shouldn't this take image rotation into account?
    x = (int)floorf(imgx);
    y = (int)floorf(imgy);

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
IvGL::print_shader_log(std::ostream& out, const GLuint shader_id)
{
    GLint size = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &size);
    if (size > 0) {
        GLchar* log = new GLchar[size];
        glGetShaderInfoLog(shader_id, size, NULL, log);
        out << "compile log:\n" << log << "---\n";
        delete[] log;
    }
}



void
IvGL::check_gl_extensions(void)
{
    m_use_shaders = hasOpenGLFeature(QOpenGLFunctions::Shaders);

    QOpenGLContext* context = QOpenGLContext::currentContext();
    QSurfaceFormat format   = context->format();
    bool isGLES = format.renderableType() == QSurfaceFormat::OpenGLES;

    m_use_srgb = (isGLES && format.majorVersion() >= 3)
                 || (!isGLES && format.version() >= qMakePair(2, 1))
                 || context->hasExtension("GL_EXT_texture_sRGB")
                 || context->hasExtension("GL_EXT_sRGB");

    m_use_halffloat = (!isGLES && format.version() >= qMakePair(3, 0))
                      || context->hasExtension("GL_ARB_half_float_pixel")
                      || context->hasExtension("GL_NV_half_float_pixel")
                      || context->hasExtension("GL_OES_texture_half_float");

    m_use_float = (!isGLES && format.version() >= qMakePair(3, 0))
                  || context->hasExtension("GL_ARB_texture_float")
                  || context->hasExtension("GL_ATI_texture_float")
                  || context->hasExtension("GL_OES_texture_float");

    m_max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_max_texture_size);
    // FIXME: Need a smarter way to handle (video) memory.
    // Don't assume that systems capable of using 8k^2 textures have enough
    // resources to use more than one of those at the same time.
    m_max_texture_size = std::min(m_max_texture_size, 4096);

#ifndef NDEBUG
    // Report back...
    std::cerr << "OpenGL Shading Language supported: " << m_use_shaders << "\n";
    std::cerr << "OpenGL sRGB color space textures supported: " << m_use_srgb
              << "\n";
    std::cerr << "OpenGL half-float pixels supported: " << m_use_halffloat
              << "\n";
    std::cerr << "OpenGL float texture storage supported: " << m_use_float
              << "\n";
    std::cerr << "OpenGL max texture dimension: " << m_max_texture_size << "\n";
#endif
}



void
IvGL::typespec_to_opengl(const ImageSpec& spec, int nchannels, GLenum& gltype,
                         GLenum& glformat, GLenum& glinternalformat) const
{
    switch (spec.format.basetype) {
    case TypeDesc::FLOAT: gltype = GL_FLOAT; break;
    case TypeDesc::HALF:
        if (m_use_halffloat) {
            gltype = GL_HALF_FLOAT_ARB;
        } else {
            // If we reach here then something really wrong happened: When
            // half-float is not supported, the image should be loaded as
            // UINT8 (no GLSL support) or FLOAT (GLSL support).
            // See IvImage::loadCurrentImage()
            std::cerr << "Tried to load an unsupported half-float image.\n";
            gltype = GL_INVALID_ENUM;
        }
        break;
    case TypeDesc::INT: gltype = GL_INT; break;
    case TypeDesc::UINT: gltype = GL_UNSIGNED_INT; break;
    case TypeDesc::INT16: gltype = GL_SHORT; break;
    case TypeDesc::UINT16: gltype = GL_UNSIGNED_SHORT; break;
    case TypeDesc::INT8: gltype = GL_BYTE; break;
    case TypeDesc::UINT8: gltype = GL_UNSIGNED_BYTE; break;
    default:
        gltype = GL_UNSIGNED_BYTE;  // punt
        break;
    }

    bool issrgb = Strutil::iequals(spec.get_string_attribute("oiio:ColorSpace"),
                                   "sRGB");

    glinternalformat = nchannels;
    if (nchannels == 1) {
        glformat = GL_LUMINANCE;
        if (m_use_srgb && issrgb) {
            if (spec.format.basetype == TypeDesc::UINT8) {
                glinternalformat = GL_SLUMINANCE8;
            } else {
                glinternalformat = GL_SLUMINANCE;
            }
        } else if (spec.format.basetype == TypeDesc::UINT8) {
            glinternalformat = GL_LUMINANCE8;
        } else if (spec.format.basetype == TypeDesc::UINT16) {
            glinternalformat = GL_LUMINANCE16;
        } else if (m_use_float && spec.format.basetype == TypeDesc::FLOAT) {
            glinternalformat = GL_LUMINANCE32F_ARB;
        } else if (m_use_float && spec.format.basetype == TypeDesc::HALF) {
            glinternalformat = GL_LUMINANCE16F_ARB;
        }
    } else if (nchannels == 2) {
        glformat = GL_LUMINANCE_ALPHA;
        if (m_use_srgb && issrgb) {
            if (spec.format.basetype == TypeDesc::UINT8) {
                glinternalformat = GL_SLUMINANCE8_ALPHA8;
            } else {
                glinternalformat = GL_SLUMINANCE_ALPHA;
            }
        } else if (spec.format.basetype == TypeDesc::UINT8) {
            glinternalformat = GL_LUMINANCE8_ALPHA8;
        } else if (spec.format.basetype == TypeDesc::UINT16) {
            glinternalformat = GL_LUMINANCE16_ALPHA16;
        } else if (m_use_float && spec.format.basetype == TypeDesc::FLOAT) {
            glinternalformat = GL_LUMINANCE_ALPHA32F_ARB;
        } else if (m_use_float && spec.format.basetype == TypeDesc::HALF) {
            glinternalformat = GL_LUMINANCE_ALPHA16F_ARB;
        }
    } else if (nchannels == 3) {
        glformat = GL_RGB;
        if (m_use_srgb && issrgb) {
            if (spec.format.basetype == TypeDesc::UINT8) {
                glinternalformat = GL_SRGB8;
            } else {
                glinternalformat = GL_SRGB;
            }
        } else if (spec.format.basetype == TypeDesc::UINT8) {
            glinternalformat = GL_RGB8;
        } else if (spec.format.basetype == TypeDesc::UINT16) {
            glinternalformat = GL_RGB16;
        } else if (m_use_float && spec.format.basetype == TypeDesc::FLOAT) {
            glinternalformat = GL_RGB32F_ARB;
        } else if (m_use_float && spec.format.basetype == TypeDesc::HALF) {
            glinternalformat = GL_RGB16F_ARB;
        }
    } else if (nchannels == 4) {
        glformat = GL_RGBA;
        if (m_use_srgb && issrgb) {
            if (spec.format.basetype == TypeDesc::UINT8) {
                glinternalformat = GL_SRGB8_ALPHA8;
            } else {
                glinternalformat = GL_SRGB_ALPHA;
            }
        } else if (spec.format.basetype == TypeDesc::UINT8) {
            glinternalformat = GL_RGBA8;
        } else if (spec.format.basetype == TypeDesc::UINT16) {
            glinternalformat = GL_RGBA16;
        } else if (m_use_float && spec.format.basetype == TypeDesc::FLOAT) {
            glinternalformat = GL_RGBA32F_ARB;
        } else if (m_use_float && spec.format.basetype == TypeDesc::HALF) {
            glinternalformat = GL_RGBA16F_ARB;
        }
    } else {
        glformat         = GL_INVALID_ENUM;
        glinternalformat = GL_INVALID_ENUM;
    }
}



void
IvGL::load_texture(int x, int y, int width, int height)
{
    const ImageSpec& spec = m_current_image->spec();
    // Find if this has already been loaded.
    for (auto&& tb : m_texbufs) {
        if (tb.x == x && tb.y == y && tb.width >= width
            && tb.height >= height) {
            glBindTexture(GL_TEXTURE_2D, tb.tex_object);
            return;
        }
    }

    setCursor(Qt::WaitCursor);

    int nchannels = spec.nchannels;
    // For simplicity, we don't support more than 4 channels without shaders
    // (yet).
    if (m_use_shaders) {
        nchannels = num_channels(m_viewer.current_channel(), nchannels,
                                 m_viewer.current_color_mode());
    }
    GLenum gltype, glformat, glinternalformat;
    typespec_to_opengl(spec, nchannels, gltype, glformat, glinternalformat);

    TexBuffer& tb = m_texbufs[m_last_texbuf_used];
    tb.x          = x;
    tb.y          = y;
    tb.width      = width;
    tb.height     = height;
    // Copy the imagebuf pixels we need, that's the only way we can do
    // it safely since ImageBuf has a cache underneath and the whole image
    // may not be resident at once.
    if (!m_use_shaders) {
        m_current_image->get_pixels(ROI(x, x + width, y, y + height),
                                    spec.format, &m_tex_buffer[0]);
    } else {
        m_current_image->get_pixels(ROI(x, x + width, y, y + height, 0, 1,
                                        m_viewer.current_channel(),
                                        m_viewer.current_channel() + nchannels),
                                    spec.format, &m_tex_buffer[0]);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo_objects[m_last_pbo_used]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * spec.pixel_bytes(),
                 &m_tex_buffer[0], GL_STREAM_DRAW);
    GLERRPRINT("After buffer data");
    m_last_pbo_used = (m_last_pbo_used + 1) & 1;

    // When using PBO this is the offset within the buffer.
    void* data = 0;

    glBindTexture(GL_TEXTURE_2D, tb.tex_object);
    GLERRPRINT("After bind texture");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, gltype,
                    data);
    GLERRPRINT("After loading sub image");
    m_last_texbuf_used = (m_last_texbuf_used + 1) % m_texbufs.size();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}



bool
IvGL::is_too_big(float width, float height)
{
    unsigned int tiles = (unsigned int)(ceilf(width / m_max_texture_size)
                                        * ceilf(height / m_max_texture_size));
    return tiles > m_texbufs.size();
}
