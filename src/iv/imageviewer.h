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



#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <vector>

#include <QtGui>


#include "imageio.h"
using namespace OpenImageIO;

class IvMainWindow;



class IvImage {
public:
    IvImage (const std::string &filename);
    ~IvImage ();

    /// Read the file from disk.  Generally will skip the read if we've
    /// already got a current version of the image in memory, unless
    /// force==true.
    bool read (bool force=false,
               OpenImageIO::ProgressCallback progress_callback=NULL,
               void *progress_callback_data=NULL);

    /// Initialize this IvImage with the named image file, and read its
    /// header to fill out the spec correctly.  Return true if this
    /// succeeded, false if the file could not be read.
    bool init_spec (const std::string &filename);

    /// Return info on the last error that occurred since error_message()
    /// was called.  This also clears the error message for next time.
    std::string error_message (void) {
        std::string e = m_err;
        m_err.clear();
        return e;
    }

    /// Return a reference to the image spec;
    ///
    const ImageIOFormatSpec & spec () const { return m_spec; }

    /// Return a pointer to the start of scanline #y.
    ///
    void *scanline (int y) {
        return (void *) (m_pixels + y * m_spec.scanline_bytes());
    }

    const std::string & name (void) const { return m_name; }

    float gamma (void) const { return m_gamma; }
    void gamma (float e) { m_gamma = e; }
    float exposure (void) const { return m_exposure; }
    void exposure (float e) { m_exposure = e; }

private:
    std::string m_name;        ///< Filename of the image
    int m_nsubimages;          ///< How many subimages are there?
    int m_current_subimage;    ///< Current subimage we're viewing
    ImageIOFormatSpec m_spec;  ///< Describes the image (size, etc)
    char *m_pixels;            ///< Pixel data
    char *m_thumbnail;         ///< Thumbnail image
    bool m_spec_valid;         ///< Is the spec valid
    bool m_pixels_valid;       ///< Image is valid
    bool m_thumbnail_valid;    ///< Thumbnail is valid
    bool m_badfile;            ///< File not found
    std::string m_err;         ///< Last error message
    float m_gamma;             ///< Gamma correction of this image
    float m_exposure;          ///< Exposure gain of this image, in stops

    // An IvImage can be in one of several states:
    //   * Uninitialized
    //         (m_name.empty())
    //   * Broken -- couldn't ever open the file
    //         (m_badfile == true)
    //   * Non-resident, ignorant -- know the name, nothing else
    //         (! m_name.empty() && ! m_badfile && ! m_spec_valid)
    //   * Non-resident, know spec, but the spec is valid
    //         (m_spec_valid && ! m_pixels)
    //   * Pixels loaded from disk, currently accurate
    //         (m_pixels && m_pixels_valid)

};



class ImageViewer : public QMainWindow
{
    Q_OBJECT

public:
    ImageViewer();
    ~ImageViewer();

    /// Tell the viewer about an image, but don't load it yet.  If
    /// getspec is true, open the file just enough to get the
    /// specification.
    void add_image (const std::string &filename, bool getspec=true);

private slots:
    void open();
    void print();
    void zoomIn();
    void zoomOut();
    void normalSize();
    void fitToWindow();
    void about();
    void prevImage();
    void nextImage();
    void exposureMinusOneTenthStop();
    void exposureMinusOneHalfStop();
    void exposurePlusOneTenthStop();
    void exposurePlusOneHalfStop();
    void gammaMinus();
    void gammaPlus();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    void updateActions();
    void scaleImage(double factor);
    void adjustScrollBar(QScrollBar *scrollBar, double factor);
    void displayCurrentImage();
    void keyPressEvent (QKeyEvent *event);

    QLabel *imageLabel;
    QScrollArea *scrollArea;
    double scaleFactor;

#ifndef QT_NO_PRINTER
    QPrinter printer;
#endif

    QAction *openAct;
    QAction *printAct;
    QAction *exitAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *fitToWindowAct;
    QAction *aboutAct;
    QAction *nextImageAct, *prevImageAct;
    QMenu *fileMenu, *editMenu, *imageMenu, *viewMenu, *toolsMenu, *helpMenu;
    QLabel *statusImgInfo, *statusViewInfo;;
    QProgressBar *statusProgress;

    std::vector<IvImage *> m_images;  ///< List of images
    int m_current_image;              ///< Index of current image, -1 if none

    friend class IvScrollArea;
    friend bool image_progress_callback (void *opaque, float done);
};


#endif // IMAGEVIEWER_H
