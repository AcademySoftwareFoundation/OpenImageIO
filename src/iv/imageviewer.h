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


#ifndef OPENIMAGEIO_IMAGEVIEWER_H
#define OPENIMAGEIO_IMAGEVIEWER_H

#if defined (_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
// Also ignore warnings about not being able to generate default assignment
// operators for some Qt classes included in headers below.
#  pragma warning (disable : 4127 4512)
#endif

// included to remove std::min/std::max errors
#include "osdep.h"

#include <vector>

// This needs to be included before GL.h
#include <glew.h>

#include <QtGui/QAction>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QMainWindow>
#include <QtOpenGL/QGLWidget>

#ifndef QT_NO_PRINTER
#include <QtGui/QPrinter>
#endif

#include "imageio.h"
#include "imagebuf.h"

OIIO_NAMESPACE_USING;

class QComboBox;
class QLabel;
class QMenu;
class QMenuBar;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QScrollArea;
class QStatusBar;
class QVBoxLayout;

class IvMainWindow;
class IvInfoWindow;
class IvPreferenceWindow;
class IvCanvas;
class IvGL;
class IvImage;

class IvImage : public ImageBuf {
public:
    IvImage (const std::string &filename);
    virtual ~IvImage ();

    /// Read the image into ram.
    /// If secondary buffer is true, and the format is UINT8, then a secondary
    /// buffer will be created and the apply_corrections(), and
    /// select_channel() methods will work.
    /// Also, scanline will return a pointer to that buffer instead of the read
    /// buffer.
    virtual bool read (int subimage=0, int miplevel=0,
                       bool force=false, TypeDesc format = TypeDesc::UNKNOWN,
                       ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL, bool secondary_buffer=false);
    virtual bool init_spec (const std::string &filename,
                            int subimage, int miplevel);

    float gamma (void) const { return m_gamma; }
    void gamma (float e) { m_gamma = e; }
    float exposure (void) const { return m_exposure; }
    void exposure (float e) { m_exposure = e; }

    int nchannels () const {
        if (m_corrected_image.localpixels()) {
            return m_corrected_image.nchannels();
        }
        return m_spec.nchannels;
    }

    std::string shortinfo () const;
    std::string longinfo () const;

    void invalidate ();

    /// Can we read the pixels of this image already?
    ///
    bool image_valid () const { return m_image_valid; }

    /// Copies data from the read buffer to the secondary buffer, selecting the
    /// given channel:
    ///  -2 = luminance
    ///  -1 = all channels
    ///   0 = red
    ///   1 = green
    ///   2 = blue
    ///   3 = alpha
    /// Then applies gamma/exposure correction (if any). This only works when
    /// the image is UINT8 (for now at least). It also performs sRGB to linear
    /// color space correction when indicated.
    void pixel_transform (bool srgb_to_linear, int color_mode, int channel);

    bool copy_pixels (int xbegin, int xend, int ybegin, int yend,
                      TypeDesc format, void *result) {
        if (m_corrected_image.localpixels ()) {
            return m_corrected_image.copy_pixels (xbegin, xend, ybegin, yend,
                                                  format, result);
        }
        return ImageBuf::copy_pixels (xbegin, xend, ybegin, yend, format, result);
    }

    bool auto_subimage (void) const { return m_auto_subimage; }
    void auto_subimage (bool v) { m_auto_subimage = v; }

    /// Copies a rectangular block of pixels into 'result' with the given
    /// 'format', copying only from channels [chbegin, chend).
    //FIXME: I really feel this should go somewhere else (i.e., in ImageBuf or as
    //an ImageBufAlgo).
    bool copy_pixel_channels  (int xbegin, int xend, int ybegin, int yend,
                       int chbegin, int chend, TypeDesc format, void *result) const;
private:
    ImageBuf m_corrected_image; ///< Colorspace/gamma/exposure corrected image.
    char *m_thumbnail;         ///< Thumbnail image
    bool m_thumbnail_valid;    ///< Thumbnail is valid
    float m_gamma;             ///< Gamma correction of this image
    float m_exposure;          ///< Exposure gain of this image, in stops
    TypeDesc m_file_dataformat; ///< TypeDesc of the image on disk (not in ram)
    mutable std::string m_shortinfo;
    mutable std::string m_longinfo;
    bool m_image_valid;        ///< Image is valid and pixels can be read.
    bool m_auto_subimage;      ///< Automatically use subimages when zooming-in/out.
};



class ImageViewer : public QMainWindow
{
    Q_OBJECT

public:
    ImageViewer();
    ~ImageViewer();

    enum COLOR_MODE {
        RGBA = 0,
        RGB = 1,
        SINGLE_CHANNEL = 2,
        LUMINANCE = 3,
        HEATMAP = 4
    };

    /// Tell the viewer about an image, but don't load it yet.
    void add_image (const std::string &filename);

    /// View this image.
    ///
    void current_image (int newimage);

    /// Which image index are we viewing?
    ///
    int current_image (void) const { return m_current_image; }

    /// View slide show (cycle through images with timed interval)
    ///
    void slide (long t, bool b);

    /// View a particular channel
    ///
    void viewChannel (int channel, COLOR_MODE colormode);

    /// Which channel are we viewing?
    ///
    int current_channel (void) const { return m_current_channel; }

    /// In what color mode are we?
    ///
    COLOR_MODE current_color_mode (void) const { return m_color_mode; }

    /// Return the current zoom level.  1.0 == 1:1 pixel ratio.  Positive
    /// is a "zoom in" (closer/maxify), negative is zoom out (farther/minify).
    float zoom (void) const { return m_zoom; }

    /// Set a new view (zoom level and center position).  If smooth is
    /// true, switch to the new view smoothly over many gradual steps,
    /// otherwise do it all in one step.  The center position is measured
    /// in pixel coordinates.
    void view (float xcenter, float ycenter, float zoom, bool smooth=false, bool redraw=true);

    /// Set a new zoom level, keeping the center of view.  If smooth is
    /// true, switch to the new zoom level smoothly over many gradual
    /// steps, otherwise do it all in one step.
    void zoom (float newzoom, bool smooth=false);

    /// Return a ptr to the current image, or NULL if there is no
    /// current image.
    IvImage *cur (void) const {
        if (m_images.empty())
            return NULL;
        return m_current_image >= 0 ? m_images[m_current_image] : NULL;
    }

    /// Return a ref to the current image spec, or NULL if there is no
    /// current image.
    const ImageSpec *curspec (void) const {
        IvImage *img = cur();
        return img ? &img->spec() : NULL;
    }

    bool pixelviewOn (void) const {
        return showPixelviewWindowAct && showPixelviewWindowAct->isChecked();
    }

    bool pixelviewFollowsMouse (void) const {
        return pixelviewFollowsMouseBox && pixelviewFollowsMouseBox->isChecked();
    }

    bool linearInterpolation (void) const {
        return linearInterpolationBox && linearInterpolationBox->isChecked();
    }

    bool darkPalette (void) const {
        return darkPaletteBox ? darkPaletteBox->isChecked() : m_darkPalette;
    }

    QPalette palette (void) const { return m_palette; }

private slots:
    void open();                        ///< Dialog to open new image from file
    void reload();                      ///< Reread current image from disk
    void openRecentFile();              ///< Open a recent file
    void closeImg();                    ///< Close the current image
    void saveAs();                      ///< Save As... functionality
    void saveWindowAs();                ///< Save As... functionality
    void saveSelectionAs();             ///< Save As... functionality
    void print();                       ///< Print current image
    void deleteCurrentImage();          ///< Deleting displayed image
    void zoomIn();                      ///< Zoom in to next power of 2
    void zoomOut();                     ///< Zoom out to next power of 2
    void normalSize();                  ///< Adjust zoom to 1:1
    void fitImageToWindow();            ///< Adjust zoom to fit window exactly
    /// Resize window to fit image exactly.  If zoomok is false, do not
    /// change the zoom, even to fit on screen. If minsize is true, do not
    /// resize smaller than default_width x default_height.
    void fitWindowToImage(bool zoomok=true, bool minsize=false);
    void fullScreenToggle();            ///< Toggle full screen mode
    void about();                       ///< Show "about iv" dialog
    void prevImage();                   ///< View previous image in sequence
    void nextImage();                   ///< View next image in sequence
    void toggleImage();                 ///< View most recently viewed image
    void exposureMinusOneTenthStop();   ///< Decrease exposure 1/10 stop
    void exposureMinusOneHalfStop();    ///< Decrease exposure 1/2 stop
    void exposurePlusOneTenthStop();    ///< Increase exposure 1/10 stop
    void exposurePlusOneHalfStop();     ///< Increase exposure 1/2 stop
    void gammaMinus();                  ///< Decrease gamma 0.05
    void gammaPlus();                   ///< Increase gamma 0.05
    void viewChannelFull();             ///< View RGB
    void viewChannelRed();              ///< View just red as gray
    void viewChannelGreen();            ///< View just green as gray
    void viewChannelBlue();             ///< View just blue as gray
    void viewChannelAlpha();            ///< View alpha as gray
    void viewChannelLuminance();        ///< View current 3 channels as luminance
    void viewChannelPrev();             ///< View just prev channel as gray
    void viewChannelNext();             ///< View just next channel as gray
    void viewColorRGBA();               ///< View current 4 channels as RGBA
    void viewColorRGB();                ///< View current 3 channels as RGB
    void viewColor1Ch();                ///< View current channel as gray
    void viewColorHeatmap();            ///< View current channel as heatmap.
    void viewSubimagePrev();            ///< View prev subimage
    void viewSubimageNext();            ///< View next subimage
    void sortByName();                  ///< Sort images by Name.
    void sortByPath();                  ///< Sort images based on full file path
    void sortByImageDate();             ///< Sort images by metadata date
    void sortByFileDate();              ///< Sort images by file Date Stamp.
    void sortReverse();                 ///< Reverse the current order of images
    void slideShow();                   ///< Starts slide show
    void slideLoop();                   ///< Slide show in a loop
    void slideNoLoop();                 ///< Slide show without loop
    void setSlideShowDuration(int seconds); ///< Set the slide show duration in seconds
    void slideImages();                 ///< Slide show - move to next image
    void showInfoWindow();              ///< View extended info on image
    void showPixelviewWindow();         ///< View closeup pixel view
    void editPreferences();             ///< Edit viewer preferences
private:
    void createActions ();
    void createMenus ();
    void createToolBars ();
    void createStatusBar ();
    void readSettings (bool ui_is_set_up=true);
    void writeSettings ();
    void updateActions ();
    void addRecentFile (const std::string &name);
    void removeRecentFile (const std::string &name);
    void updateRecentFilesMenu ();
    bool loadCurrentImage (int subimage = 0, int miplevel = 0);
    void displayCurrentImage (bool update = true);
    void updateTitle ();
    void updateStatusBar ();
    void keyPressEvent (QKeyEvent *event);
    void resizeEvent (QResizeEvent *event);
    void closeEvent (QCloseEvent *event);

    QTimer *slideTimer;          ///< Timer to use for slide show mode
    long slideDuration_ms;       ///< Slide show mode duration (in ms)
    bool slide_loop;             ///< Do we loop when in slide mode?

    IvGL *glwin;
    IvInfoWindow *infoWindow;
    IvPreferenceWindow *preferenceWindow;

#ifndef QT_NO_PRINTER
    QPrinter printer;
#endif

    QAction *openAct, *reloadAct, *closeImgAct;
    static const unsigned int MaxRecentFiles = 10;
    QAction *openRecentAct[MaxRecentFiles];
    QAction *saveAsAct, *saveWindowAsAct, *saveSelectionAsAct;
    QAction *printAct;
    QAction *deleteCurrentImageAct;
    QAction *exitAct;
    QAction *gammaPlusAct, *gammaMinusAct;
    QAction *exposurePlusOneTenthStopAct, *exposurePlusOneHalfStopAct;
    QAction *exposureMinusOneTenthStopAct, *exposureMinusOneHalfStopAct;
    QAction *viewChannelFullAct, *viewChannelRedAct, *viewChannelGreenAct;
    QAction *viewChannelBlueAct, *viewChannelAlphaAct;
    QAction *viewChannelPrevAct, *viewChannelNextAct;
    QAction *viewColorRGBAAct, *viewColorRGBAct, *viewColor1ChAct;
    QAction *viewColorLumAct, *viewColorHeatmapAct;
    QAction *viewSubimagePrevAct, *viewSubimageNextAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *fitWindowToImageAct, *fitImageToWindowAct;
    QAction *fullScreenAct;
    QAction *aboutAct;
    QAction *nextImageAct, *prevImageAct, *toggleImageAct;
    QAction *sortByNameAct, *sortByPathAct, *sortReverseAct;
    QAction *sortByImageDateAct, *sortByFileDateAct;
    QAction *slideShowAct, *slideLoopAct, *slideNoLoopAct;
    QAction *showInfoWindowAct;
    QAction *editPreferencesAct;
    QAction *showPixelviewWindowAct;
    QMenu *fileMenu, *editMenu, /**imageMenu,*/ *viewMenu, *toolsMenu, *helpMenu;
    QMenu *openRecentMenu;
    QMenu *expgamMenu, *channelMenu, *colormodeMenu, *slideMenu, *sortMenu;
    QLabel *statusImgInfo, *statusViewInfo;
    QProgressBar *statusProgress;
    QComboBox *mouseModeComboBox;
    enum MouseMode { MouseModeZoom, MouseModePan, MouseModeWipe,
                     MouseModeSelect, MouseModeAnnotate };
    QCheckBox *pixelviewFollowsMouseBox;
    QCheckBox *linearInterpolationBox;
    QCheckBox *darkPaletteBox;
    QCheckBox *autoMipmap;
    QLabel   *maxMemoryICLabel;
    QSpinBox *maxMemoryIC;
    QLabel   *slideShowDurationLabel;
    QSpinBox *slideShowDuration;

    std::vector<IvImage *> m_images;  ///< List of images
    int m_current_image;              ///< Index of current image, -1 if none
    int m_current_channel;            ///< Channel we're viewing.
    COLOR_MODE m_color_mode;          ///< How to show the current channel(s).
    int m_last_image;                 ///< Last image we viewed
    float m_zoom;                     ///< Zoom amount (positive maxifies)
    bool m_fullscreen;                ///< Full screen mode
    std::vector<std::string> m_recent_files;  ///< Recently opened files
    float m_default_gamma;            ///< Default gamma of the display
    QPalette m_palette;               ///< Custom palette
    bool m_darkPalette;               ///< Use dark palette?

    static const int m_default_width = 640; ///< The default width of the window.
    static const int m_default_height = 480; ///< The default height of the window.

    // What zoom do we need to fit these window dimensions?
    float zoom_needed_to_fit (int w, int h);

    friend class IvCanvas;
    friend class IvGL;
    friend class IvInfoWindow;
    friend class IvPreferenceWindow;
    friend bool image_progress_callback (void *opaque, float done);
};



class IvInfoWindow : public QDialog
{
    Q_OBJECT
public:
    IvInfoWindow (ImageViewer &viewer, bool visible=true);
    void update (IvImage *img);

protected:
    void keyPressEvent (QKeyEvent *event);
    
private:
    QPushButton *closeButton;
    QScrollArea *scrollArea;
    QLabel *infoLabel;

    ImageViewer &m_viewer;
    bool m_visible;
};



class IvPreferenceWindow : public QDialog
{
    Q_OBJECT
public:
    IvPreferenceWindow (ImageViewer &viewer);

protected:
    void keyPressEvent (QKeyEvent *event);
    
private:
    QVBoxLayout *layout;
    QPushButton *closeButton;

    ImageViewer &m_viewer;
};



class IvGL : public QGLWidget
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
    void zoom (float newzoom, bool redraw=true) {
        view (m_centerx, m_centery, newzoom, redraw);
    }

    /// Update just the center (in pixel coordinates), keep the old zoom.
    ///
    void center (float x, float y, bool redraw=true) {
        view (x, y, m_viewer.zoom(), redraw);
    }

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

    void trigger_redraw (void) { glDraw(); }

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
    bool m_shaders_using_extensions;  ///< Are we using ARB_*_shader?
    bool m_use_halffloat;             ///< Are half-float textures supported?
    bool m_use_float;                 ///< Are float textures supported?
    bool m_use_srgb;                  ///< Are sRGB-space textures supported?
    bool m_use_npot_texture;          ///< Can we handle NPOT textures?
    bool m_use_pbo;                   ///< Can we use PBO to upload the texture?
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
    typedef QGLWidget parent_t;
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

    // Small wrappers to handle ARB shaders.
    void gl_use_program (int program);
    GLint gl_get_uniform_location (const char*);
    void gl_uniform (GLint location, float value);
    void gl_uniform (GLint location, int value);

    /// checks what OpenGL extensions we have
    ///
    void check_gl_extensions (void);

    /// print shader info to out stream
    ///
    void print_shader_log (std::ostream& out, const GLuint shader_id) const;

    /// Loads the given patch of the image, but first figures if it's already
    /// been loaded.
    void load_texture (int x, int y, int width, int height, float percent);
    
    /// Destroys shaders and selects fixed-function pipeline
    void create_shaders_abort (void);
};

#endif // OPENIMAGEIO_IMAGEVIEWER_H
