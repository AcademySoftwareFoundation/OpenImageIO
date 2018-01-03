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


#ifndef OPENIMAGEIO_IVGL_H
#define OPENIMAGEIO_IVGL_H

#if defined (_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
// Also ignore warnings about not being able to generate default assignment
// operators for some Qt classes included in headers below.
#  pragma warning (disable : 4127 4512)
#endif

// included to remove std::min/std::max errors
#include <OpenImageIO/platform.h>

#include <vector>

#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

using namespace OIIO;

class IvImage;
class ImageViewer;



class IvGL : public QOpenGLWidget, protected QOpenGLFunctions
{
Q_OBJECT
public:
    IvGL (QWidget *parent, ImageViewer &viewer);
    virtual ~IvGL ();

    /// Update the image texture.
    ///
    virtual void update ();

    /// Update the view -- center (in pixel coordinates) and zoom level.
    ///
    virtual void view (float centerx, float centery, float zoom,
                       bool redraw=true);

    /// Update just the zoom, keep the old center
    ///
    void zoom (float newzoom, bool redraw=true);

    /// Update just the center (in pixel coordinates), keep the old zoom.
    ///
    void center (float x, float y, bool redraw=true);

    /// Get the center of the view, in pixel coordinates.
    ///
    void get_center (float &x, float &y) {
        x = m_centerx;
        y = m_centery;
    }

    void pan (float dx, float dy);

    /// Let the widget know which pixel the mouse is over
    ///
    void remember_mouse (const QPoint &pos);

    /// Which image pixel is the mouse over?
    ///
    void get_focus_image_pixel (int &x, int &y);

    /// Which display window pixel is the mouse over?  (Relative to
    /// widget boundaries)
    void get_focus_window_pixel (int &x, int &y);

    /// Returns true if OpenGL is capable of loading textures in the sRGB color
    /// space.
    bool is_srgb_capable (void) const { return m_use_srgb; }

    /// Returns true if OpenGL can use GLSL, either with extensions or as
    /// implementation of version 2.0
    bool is_glsl_capable (void) const { return m_use_shaders; }

    /// Is OpenGL capable of reading half-float textures?
    ///
    bool is_half_capable (void) const { return m_use_halffloat; }

    /// Returns true if the image is too big to fit within allocated textures
    /// (i.e., it's recommended to use lower resolution versions when zoomed out).
    bool is_too_big (float width, float height);

    void typespec_to_opengl (const ImageSpec& spec, int nchannels, GLenum &gltype,
                             GLenum &glformat, GLenum &glinternal) const;

protected:
    ImageViewer &m_viewer;            ///< Backpointer to viewer
    bool m_shaders_created;           ///< Have the shaders been created?
    GLuint m_vertex_shader;           ///< Vertex shader id
    GLuint m_fragment_shader;         ///< Fragment shader id
    GLuint m_shader_program;          ///< GL shader program id
    bool m_tex_created;               ///< Have the textures been created?
    float m_zoom;                     ///< Zoom ratio
    float m_centerx, m_centery;       ///< Center of view, in pixel coords
    bool m_dragging;                  ///< Are we dragging?
    int m_mousex, m_mousey;           ///< Last mouse position
    Qt::MouseButton m_drag_button;    ///< Button on when dragging
    bool m_use_shaders;               ///< Are shaders supported?
    bool m_use_halffloat;             ///< Are half-float textures supported?
    bool m_use_float;                 ///< Are float textures supported?
    bool m_use_srgb;                  ///< Are sRGB-space textures supported?
    bool m_use_npot_texture;          ///< Can we handle NPOT textures?
    GLint m_max_texture_size;         ///< Maximum allowed texture dimension.
    GLsizei m_texture_width;
    GLsizei m_texture_height;
    GLuint m_pbo_objects[2];          ///< Pixel buffer objects
    int m_last_pbo_used;              ///< Last used pixel buffer object.
    IvImage *m_current_image;         ///< Image to show on screen.
    GLuint m_pixelview_tex;           ///< Pixelview's own texture.
    bool m_pixelview_left_corner;     ///< Draw pixelview in upper left or right
    /// Buffer passed to IvImage::copy_image when not using PBO.
    ///
    std::vector<unsigned char> m_tex_buffer;

    /// Represents a texture object being used as a buffer.
    ///
    struct TexBuffer {
        GLuint tex_object;
        int x;
        int y;
        int width;
        int height;
    };
    std::vector<TexBuffer> m_texbufs;
    int m_last_texbuf_used;
    bool m_mouse_activation;          ///< Can we expect the window to be activated by mouse?


    virtual void initializeGL ();
    virtual void resizeGL (int w, int h);
    virtual void paintGL ();

    virtual void mousePressEvent (QMouseEvent *event);
    virtual void mouseReleaseEvent (QMouseEvent *event);
    virtual void mouseMoveEvent (QMouseEvent *event);
    virtual void wheelEvent (QWheelEvent *event);
    virtual void focusOutEvent (QFocusEvent *event);

    void paint_pixelview ();
    void glSquare (float xmin, float ymin, float xmax, float ymax, float z=0);

    virtual void create_shaders (void);
    virtual void create_textures (void);
    virtual void useshader (int tex_width, int tex_height, bool pixelview=false);

    void shadowed_text (float x, float y, float z, const std::string &s,
                        const QFont &font);

private:
    typedef QOpenGLWidget parent_t;
    /// ncloseuppixels is the number of big pixels (in each direction)
    /// visible in our closeup window.
    const static int ncloseuppixels = 9;
    /// closeuppixelzoom is the zoom factor we use for closeup pixels --
    /// i.e. one image pixel will appear in the closeup window as a 
    /// closeuppixelzoom x closeuppixelzoom square.
    const static int closeuppixelzoom = 24;
    /// closeupsize is the size, in pixels, of the closeup window itself --
    /// just the number of pixels times the width of each closeup pixel.
    const static int closeupsize = ncloseuppixels * closeuppixelzoom;
    /// closeuptexsize is the size of the texture used to upload the pixelview
    /// to OpenGL.
    const static int closeuptexsize = 16;

    void clamp_view_to_window ();

    /// checks what OpenGL extensions we have
    ///
    void check_gl_extensions (void);

    /// print shader info to out stream
    ///
    void print_shader_log (std::ostream& out, const GLuint shader_id);

    /// Loads the given patch of the image, but first figures if it's already
    /// been loaded.
    void load_texture (int x, int y, int width, int height, float percent);
    
    /// Destroys shaders and selects fixed-function pipeline
    void create_shaders_abort (void);
};

#endif // OPENIMAGEIO_IVGL_H
