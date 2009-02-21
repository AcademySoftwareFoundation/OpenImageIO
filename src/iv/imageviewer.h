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


#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <vector>

#include <QtGui>
#include <QGLWidget>

#include "imageio.h"
#include "imagebuf.h"
using namespace OpenImageIO;

class IvMainWindow;
class IvInfoWindow;
class IvPreferenceWindow;
class IvCanvas;
class IvGL;



class IvImage : public ImageBuf {
public:
    IvImage (const std::string &filename);
    virtual ~IvImage ();

    virtual bool read (int subimage=0, bool force=false,
                       OpenImageIO::ProgressCallback progress_callback=NULL,
                       void *progress_callback_data=NULL);
    virtual bool init_spec (const std::string &filename);

    float gamma (void) const { return m_gamma; }
    void gamma (float e) { m_gamma = e; }
    float exposure (void) const { return m_exposure; }
    void exposure (float e) { m_exposure = e; }

    std::string shortinfo () const;
    std::string longinfo () const;

    void invalidate () { m_pixels_valid = false;  m_thumbnail_valid = false; }

private:
    char *m_thumbnail;         ///< Thumbnail image
    bool m_thumbnail_valid;    ///< Thumbnail is valid
    float m_gamma;             ///< Gamma correction of this image
    float m_exposure;          ///< Exposure gain of this image, in stops
    mutable std::string m_shortinfo;
    mutable std::string m_longinfo;
};



class ImageViewer : public QMainWindow
{
    Q_OBJECT

public:
    ImageViewer();
    ~ImageViewer();

    enum ChannelView {
        channelRed=0, channelGreen=1, channelBlue=2, channelAlpha=3,
        channelFullColor = -1, channelLuminance = -2
    };

    /// Tell the viewer about an image, but don't load it yet.
    void add_image (const std::string &filename);

    /// View this image.
    ///
    void current_image (int newimage);

    /// Which image index are we viewing?
    ///
    int current_image (void) const { return m_current_image; }

    /// View a particular channel
    ///
    void viewChannel (ChannelView c);

    /// Which channel are we viewing?
    ///
    int current_channel (void) const { return m_current_channel; }

    /// Return the current zoom level.  1.0 == 1:1 pixel ratio.  Positive
    /// is a "zoom in" (closer/maxify), negative is zoom out (farther/minify).
    float zoom (void) const { return m_zoom; }

    /// Set a new view (zoom level and center position).  If smooth is
    /// true, switch to the new view smoothly over many gradual steps,
    /// otherwise do it all in one step.  The center position is measured
    /// in pixel coordinates.
    void view (float xcenter, float ycenter, float zoom, bool smooth=false);

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
    void zoomIn();                      ///< Zoom in to next power of 2
    void zoomOut();                     ///< Zoom out to next power of 2
    void normalSize();                  ///< Adjust zoom to 1:1
    void fitImageToWindow();            ///< Adjust zoom to fit window exactly
    /// Resize window to fit image exactly.  If zoomok is false, do not
    /// change the zoom, even to fit on screen.
    void fitWindowToImage(bool zoomok=true);
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
    void viewChannelLuminance();        ///< View luminance as gray
    void viewChannelPrev();             ///< View just prev channel as gray
    void viewChannelNext();             ///< View just next channel as gray
    void viewSubimagePrev();            ///< View prev subimage
    void viewSubimageNext();            ///< View next subimage
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
    void displayCurrentImage ();
    void updateTitle ();
    void updateStatusBar ();
    void keyPressEvent (QKeyEvent *event);
    void resizeEvent (QResizeEvent *event);
    void closeEvent (QCloseEvent *event);

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
    QAction *exitAct;
    QAction *gammaPlusAct, *gammaMinusAct;
    QAction *exposurePlusOneTenthStopAct, *exposurePlusOneHalfStopAct;
    QAction *exposureMinusOneTenthStopAct, *exposureMinusOneHalfStopAct;
    QAction *viewChannelFullAct, *viewChannelRedAct, *viewChannelGreenAct;
    QAction *viewChannelBlueAct, *viewChannelAlphaAct, *viewChannelLuminanceAct;
    QAction *viewChannelPrevAct, *viewChannelNextAct;
    QAction *viewSubimagePrevAct, *viewSubimageNextAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *fitWindowToImageAct, *fitImageToWindowAct;
    QAction *fullScreenAct;
    QAction *aboutAct;
    QAction *nextImageAct, *prevImageAct, *toggleImageAct;
    QAction *showInfoWindowAct;
    QAction *editPreferencesAct;
    QAction *showPixelviewWindowAct;
    QMenu *fileMenu, *editMenu, /**imageMenu,*/ *viewMenu, *toolsMenu, *helpMenu;
    QMenu *openRecentMenu;
    QMenu *expgamMenu, *channelMenu;
    QLabel *statusImgInfo, *statusViewInfo;
    QProgressBar *statusProgress;
    QComboBox *mouseModeComboBox;
    enum MouseMode { MouseModeZoom, MouseModePan, MouseModeWipe,
                     MouseModeSelect, MouseModeAnnotate };
    QCheckBox *pixelviewFollowsMouseBox;
    QCheckBox *linearInterpolationBox;
    QCheckBox *darkPaletteBox;

    std::vector<IvImage *> m_images;  ///< List of images
    int m_current_image;              ///< Index of current image, -1 if none
    int m_current_channel;            ///< Channel we're viewing: ChannelViews
    int m_last_image;                 ///< Last image we viewed
    float m_zoom;                     ///< Zoom amount (positive maxifies)
    bool m_fullscreen;                ///< Full screen mode
    std::vector<std::string> m_recent_files;  ///< Recently opened files
    float m_default_gamma;            ///< Default gamma of the display
    QPalette m_palette;               ///< Custom palette
    bool m_darkPalette;               ///< Use dark palette?

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
    virtual void update (IvImage *img);

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

protected:
    ImageViewer &m_viewer;            ///< Backpointer to viewer
    bool m_shaders_created;           ///< Have the shaders been created?
    GLuint m_vertex_shader;           ///< Vertex shader id
    GLuint m_fragment_shader;         ///< Fragment shader id
    GLuint m_shader_program;          ///< GL shader program id
    bool m_tex_created;               ///< Have the textures been created?
    GLuint m_texid;                   ///< Texture holding current imag
    float m_zoom;                     ///< Zoom ratio
    float m_centerx, m_centery;       ///< Center of view, in pixel coords
    bool m_dragging;                  ///< Are we dragging?
    int m_mousex, m_mousey;           ///< Last mouse position
    Qt::MouseButton m_drag_button;    ///< Button on when dragging

    virtual void initializeGL ();
    virtual void resizeGL (int w, int h);
    virtual void paintGL ();

    virtual void mousePressEvent (QMouseEvent *event);
    virtual void mouseReleaseEvent (QMouseEvent *event);
    virtual void mouseMoveEvent (QMouseEvent *event);
    virtual void wheelEvent (QWheelEvent *event);

    void paint_pixelview ();
    void glSquare (float xmin, float ymin, float xmax, float ymax, float z=0);

    virtual void create_shaders (void);
    virtual void create_textures (void);
    virtual void useshader (bool pixelview=false);

    void shadowed_text (float x, float y, float z, const std::string &s,
                        const QFont &font);

private:
    typedef QGLWidget parent_t;

    void clamp_view_to_window ();
};



#endif // IMAGEVIEWER_H
