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
    IvImage ();
    ~IvImage ();

    /// Replace the image by one read from the given file.
    ///
    bool read (const std::string &filename);

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

private:
    std::string m_name;        ///< Filename of the image
    int m_nsubimages;          ///< How many subimages are there?
    int m_current_subimage;    ///< Current subimage we're viewing
    ImageIOFormatSpec m_spec;  ///< Describes the image (size, etc)
    char *m_pixels;            ///< Pixel data
    char *m_thumbnail;         ///< Thumbnail image
    bool m_pixels_valid;       ///< Image is valid
    bool m_thumbnail_valid;    ///< Thumbnail is valid
    bool m_badfile;            ///< File not found
    std::string m_err;         ///< Last error message
};



class ImageViewer : public QMainWindow
{
    Q_OBJECT

public:
    ImageViewer();
    ~ImageViewer();

private slots:
    void open();
    void print();
    void zoomIn();
    void zoomOut();
    void normalSize();
    void fitToWindow();
    void about();

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

    QMenu *fileMenu, *editMenu, *imageMenu, *viewMenu, *toolsMenu, *helpMenu;

    std::vector<IvImage *> m_images;  ///< List of images
    int m_current_image;              ///< Index of current image, -1 if none
};


#endif // IMAGEVIEWER_H
