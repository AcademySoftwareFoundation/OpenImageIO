// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#ifndef OPENIMAGEIO_IVGL_H
#define OPENIMAGEIO_IVGL_H

#if defined(_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
// Also ignore warnings about not being able to generate default assignment
// operators for some Qt classes included in headers below.
#    pragma warning(disable : 4127 4512)
#endif

// included to remove std::min/std::max errors
#include <OpenImageIO/platform.h>

#include <vector>

#include <QOpenGLExtraFunctions>
#include <QOpenGLWidget>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>

using namespace OIIO;

class IvImage;
class ImageViewer;

class IvGL : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    IvGL(QWidget* parent, ImageViewer& viewer);
    virtual ~IvGL();

    /// Update the image texture.
    ///
    virtual void update();

    /// Update the view -- center (in pixel coordinates) and zoom level.
    ///
    virtual void view(float centerx, float centery, float zoom,
                      bool redraw = true);

    /// Update just the zoom, keep the old center
    ///
    void zoom(float newzoom, bool redraw = true);

    /// Update just the center (in pixel coordinates), keep the old zoom.
    ///
    void center(float x, float y, bool redraw = true);

    /// Get the center of the view, in pixel coordinates.
    ///
    void get_center(float& x, float& y)
    {
        x = m_centerx;
        y = m_centery;
    }

    void pan(float dx, float dy);

    /// Let the widget know which pixel the mouse is over
    ///
    void remember_mouse(const QPoint& pos);

    /// Which image pixel is the mouse over?
    ///
    void get_focus_image_pixel(int& x, int& y);

    /// Which display window pixel is the mouse over?  (Relative to
    /// widget boundaries)
    void get_focus_window_pixel(int& x, int& y);

    /// Which image pixel is in the given mouse position?
    ///
    void get_given_image_pixel(int& x, int& y, int mouseX, int mouseY);

    /// What are the min/max/avg values of each channel in the selected area?
    void update_area_probe_text();

    /// Returns true if OpenGL is capable of loading textures in the sRGB color
    /// space.
    bool is_srgb_capable(void) const { return m_use_srgb; }

    /// Returns true if OpenGL can use GLSL, either with extensions or as
    /// implementation of version 2.0
    bool is_glsl_capable(void) const { return m_use_shaders; }

    /// Is OpenGL capable of reading half-float textures?
    ///
    bool is_half_capable(void) const { return m_use_halffloat; }

    /// Returns true if the image is too big to fit within allocated textures
    /// (i.e., it's recommended to use lower resolution versions when zoomed out).
    bool is_too_big(float width, float height);

    void typespec_to_opengl(const ImageSpec& spec, int nchannels,
                            GLenum& gltype, GLenum& glformat,
                            GLenum& glinternal) const;

protected:
    ImageViewer& m_viewer;          ///< Backpointer to viewer
    bool m_shaders_created;         ///< Have the shaders been created?
    GLuint m_vertex_shader;         ///< Vertex shader id
    GLuint m_shader_program;        ///< GL shader program id
    bool m_tex_created;             ///< Have the textures been created?
    float m_zoom;                   ///< Zoom ratio
    float m_centerx, m_centery;     ///< Center of view, in pixel coords
    bool m_dragging;                ///< Are we dragging?
    int m_mousex, m_mousey;         ///< Last mouse position
    Qt::MouseButton m_drag_button;  ///< Button on when dragging
    QPoint m_select_start;          ///< Mouse start position for the area probe
    QPoint m_select_end;            ///< Mouse end position for the area probe
    bool m_selecting;               ///< Are we selecting?
    bool m_use_shaders;             ///< Are shaders supported?
    bool m_use_halffloat;           ///< Are half-float textures supported?
    bool m_use_float;               ///< Are float textures supported?
    bool m_use_srgb;                ///< Are sRGB-space textures supported?
    bool m_use_npot_texture;        ///< Can we handle NPOT textures?
    GLint m_max_texture_size;       ///< Maximum allowed texture dimension.
    GLsizei m_texture_width;
    GLsizei m_texture_height;
    GLuint m_pbo_objects[2];       ///< Pixel buffer objects
    int m_last_pbo_used;           ///< Last used pixel buffer object.
    IvImage* m_current_image;      ///< Image to show on screen.
    GLuint m_pixelview_tex;        ///< Pixelview's own texture.
    bool m_pixelview_left_corner;  ///< Draw pixelview in upper left or right
    bool m_probeview_left_corner;  ///< Draw probeview in bottom left or right
    /// Buffer passed to IvImage::copy_image when not using PBO.
    ///
    std::vector<unsigned char> m_tex_buffer;

    std::string m_color_shader_text;
    std::string m_area_probe_text;

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
    bool m_mouse_activation;  ///< Can we expect the window to be activated by mouse?


    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    void paint_pixelview();
    void paint_probeview();
    void paint_windowguides();
    void glSquare(float xmin, float ymin, float xmax, float ymax, float z = 0);

    virtual void create_shaders(void);
    virtual void create_textures(void);
    virtual void useshader(int tex_width, int tex_height,
                           bool pixelview = false);

    void shadowed_text(float x, float y, float z, const std::string& s,
                       const QColor& color = Qt::white);

    virtual void update_state(void);

    virtual void use_program(void);

    virtual void update_uniforms(int tex_width, int tex_height, bool pixelview);

    void print_error(const char* msg);

    virtual const char* color_func_shader_text();

private:
    typedef QOpenGLWidget parent_t;

    /// closeup_window_size is the size, in pixels, of the closeup window itself --
    /// just the number of pixels times the width of each closeup pixel.
    const static int closeup_window_size = 260;

    /// closeup_texture_size is the size of the texture used to upload the pixelview
    /// to OpenGL. It should be as big as max number of pixels in the closeup window.
    const static int closeup_texture_size = 25;

    void clamp_view_to_window();

    /// checks what OpenGL extensions we have
    ///
    void check_gl_extensions(void);

    /// print shader info to out stream
    ///
    void print_shader_log(std::ostream& out, const GLuint shader_id);

    /// Loads the given patch of the image, but first figures if it's already
    /// been loaded.
    void load_texture(int x, int y, int width, int height);

    /// Destroys shaders and selects fixed-function pipeline
    void create_shaders_abort(void);
};

#endif  // OPENIMAGEIO_IVGL_H
