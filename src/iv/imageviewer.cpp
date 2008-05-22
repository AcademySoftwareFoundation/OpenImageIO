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
#include <cmath>

#include <boost/foreach.hpp>

#include "imageviewer.h"
#include "dassert.h"
#include "strutil.h"



/// Subclass QScrollArea just so we can intercept the keystrokes, in
/// particular the arrows that we want to page between images in our UI,
/// not to be hooked to the scroll bars.
class IvScrollArea : public QScrollArea
{
public:
    IvScrollArea (ImageViewer &viewer) : m_viewer(viewer) { }
private:
    void keyPressEvent (QKeyEvent *event);
    ImageViewer &m_viewer;
};


void
IvScrollArea::keyPressEvent (QKeyEvent *event)
{
//    std::cerr << "IvScrollArea key " << (int)event->key() << '\n';
    switch (event->key()) {
#if 1
    case Qt::Key_Left :
    case Qt::Key_Up :
    case Qt::Key_PageUp :
        m_viewer.prevImage();
        return;  //break;
    case Qt::Key_Right :
        std::cerr << "Modifier is " << (int)event->modifiers() << '\n';
        fprintf (stderr, "%x\n", (int)event->modifiers());
        if (event->modifiers() == Qt::ShiftModifier)
            std::cerr << "hey, ctrl right\n";
    case Qt::Key_Down :
    case Qt::Key_PageDown :
        m_viewer.nextImage();
        return; //break;
#endif
    }
    QScrollArea::keyPressEvent (event);
}



ImageViewer::ImageViewer ()
    : m_current_image(-1), m_current_channel(-1)
{
    imageLabel = new QLabel;
    imageLabel->setBackgroundRole (QPalette::Base);
    imageLabel->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents (true);

    scrollArea = new IvScrollArea (*this);
    scrollArea->setBackgroundRole (QPalette::Dark);
    scrollArea->setWidget (imageLabel);
    setCentralWidget (scrollArea);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    setWindowTitle(tr("Image Viewer"));
    resize(500, 400);
}



ImageViewer::~ImageViewer ()
{
    BOOST_FOREACH (IvImage *i, m_images)
        delete i;
}



void ImageViewer::createActions()
{
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    reloadAct = new QAction(tr("&Reload..."), this);
    reloadAct->setShortcut(tr("Ctrl+R"));
    connect(reloadAct, SIGNAL(triggered()), this, SLOT(reload()));

    closeImgAct = new QAction(tr("&Close Image"), this);
    closeImgAct->setShortcut(tr("Ctrl+W"));
    connect(closeImgAct, SIGNAL(triggered()), this, SLOT(closeImg()));

    printAct = new QAction(tr("&Print..."), this);
    printAct->setShortcut(tr("Ctrl+P"));
    printAct->setEnabled(false);
    connect(printAct, SIGNAL(triggered()), this, SLOT(print()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    exposurePlusOneTenthStopAct = new QAction(tr("Exposure +1/10 stop"), this);
    exposurePlusOneTenthStopAct->setShortcut(tr("]"));
    connect(exposurePlusOneTenthStopAct, SIGNAL(triggered()), this, SLOT(exposurePlusOneTenthStop()));

    exposurePlusOneHalfStopAct = new QAction(tr("Exposure +1/2 stop"), this);
    exposurePlusOneHalfStopAct->setShortcut(tr("}"));
    connect(exposurePlusOneHalfStopAct, SIGNAL(triggered()), this, SLOT(exposurePlusOneHalfStop()));

    exposureMinusOneTenthStopAct = new QAction(tr("Exposure -1/10 stop"), this);
    exposureMinusOneTenthStopAct->setShortcut(tr("["));
    connect(exposureMinusOneTenthStopAct, SIGNAL(triggered()), this, SLOT(exposureMinusOneTenthStop()));

    exposureMinusOneHalfStopAct = new QAction(tr("Exposure -1/2 stop"), this);
    exposureMinusOneHalfStopAct->setShortcut(tr("{"));
    connect(exposureMinusOneHalfStopAct, SIGNAL(triggered()), this, SLOT(exposureMinusOneHalfStop()));

    gammaPlusAct = new QAction(tr("Gamma +0.1"), this);
    gammaPlusAct->setShortcut(tr(")"));
    connect(gammaPlusAct, SIGNAL(triggered()), this, SLOT(gammaPlus()));

    gammaMinusAct = new QAction(tr("Gamma -0.1"), this);
    gammaMinusAct->setShortcut(tr("("));
    connect(gammaMinusAct, SIGNAL(triggered()), this, SLOT(gammaMinus()));

    viewChannelFullAct = new QAction(tr("Full Color"), this);
    viewChannelFullAct->setShortcut(tr("c"));
    connect(viewChannelFullAct, SIGNAL(triggered()), this, SLOT(viewChannelFull()));

    viewChannelRedAct = new QAction(tr("Red"), this);
    viewChannelRedAct->setShortcut(tr("r"));
    connect(viewChannelRedAct, SIGNAL(triggered()), this, SLOT(viewChannelRed()));

    viewChannelGreenAct = new QAction(tr("Green"), this);
    viewChannelGreenAct->setShortcut(tr("g"));
    connect(viewChannelGreenAct, SIGNAL(triggered()), this, SLOT(viewChannelGreen()));

    viewChannelBlueAct = new QAction(tr("Blue"), this);
    viewChannelBlueAct->setShortcut(tr("b"));
    connect(viewChannelBlueAct, SIGNAL(triggered()), this, SLOT(viewChannelBlue()));

    viewChannelAlphaAct = new QAction(tr("Alpha"), this);
    viewChannelAlphaAct->setShortcut(tr("a"));
    connect(viewChannelAlphaAct, SIGNAL(triggered()), this, SLOT(viewChannelAlpha()));

    viewChannelLuminanceAct = new QAction(tr("Luminance"), this);
    viewChannelLuminanceAct->setShortcut(tr("l"));
    connect(viewChannelLuminanceAct, SIGNAL(triggered()), this, SLOT(viewChannelLuminance()));

    viewChannelPrevAct = new QAction(tr("Prev Channel"), this);
    viewChannelPrevAct->setShortcut(tr(","));
    connect(viewChannelPrevAct, SIGNAL(triggered()), this, SLOT(viewChannelPrev()));

    viewChannelNextAct = new QAction(tr("Next Channel"), this);
    viewChannelNextAct->setShortcut(tr("."));
    connect(viewChannelNextAct, SIGNAL(triggered()), this, SLOT(viewChannelNext()));

    zoomInAct = new QAction(tr("Zoom &In (25%)"), this);
    zoomInAct->setShortcut(tr("Ctrl++"));
    zoomInAct->setEnabled(false);
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomOutAct = new QAction(tr("Zoom &Out (25%)"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    zoomOutAct->setEnabled(false);
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

    normalSizeAct = new QAction(tr("&Normal Size"), this);
    normalSizeAct->setShortcut(tr("Ctrl+S"));
    normalSizeAct->setEnabled(false);
    connect(normalSizeAct, SIGNAL(triggered()), this, SLOT(normalSize()));

    fitToWindowAct = new QAction(tr("&Fit to Window"), this);
    fitToWindowAct->setEnabled(false);
    fitToWindowAct->setCheckable(true);
    fitToWindowAct->setShortcut(tr("Ctrl+F"));
    connect(fitToWindowAct, SIGNAL(triggered()), this, SLOT(fitToWindow()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

#if 1
    // FIXME: why doesn't this work?
    prevImageAct = new QAction(tr("Previous Image"), this);
    prevImageAct->setShortcut(tr("PageUp"));
    prevImageAct->setEnabled(true);
    connect (prevImageAct, SIGNAL(triggered()), this, SLOT(prevImage()));

    nextImageAct = new QAction(tr("Next Image"), this);
    nextImageAct->setShortcut(tr("PageDown"));
    nextImageAct->setEnabled(true);
    connect (nextImageAct, SIGNAL(triggered()), this, SLOT(nextImage()));
#endif
}



void
ImageViewer::createMenus()
{
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction (openAct);
    fileMenu->addAction (reloadAct);
    fileMenu->addAction (closeImgAct);
    fileMenu->addAction (printAct);
    fileMenu->addSeparator ();
    // Save as ^S
    // Save window as Shift-Ctrl-S
    // Save selection as
    fileMenu->addSeparator ();
    // Preferences ^,
    fileMenu->addAction (exitAct);
    menuBar()->addMenu (fileMenu);

    editMenu = new QMenu(tr("&Edit"), this);
    // Copy
    // Paste
    // Clear selection
    // radio: prioritize selection, crop selection
    menuBar()->addMenu (editMenu);

    expgamMenu = new QMenu(tr("Exposure/gamma"));  // submenu
    expgamMenu->addAction (exposureMinusOneHalfStopAct);
    expgamMenu->addAction (exposureMinusOneTenthStopAct);
    expgamMenu->addAction (exposurePlusOneHalfStopAct);
    expgamMenu->addAction (exposurePlusOneTenthStopAct);
    expgamMenu->addAction (gammaMinusAct);
    expgamMenu->addAction (gammaPlusAct);

//    imageMenu = new QMenu(tr("&Image"), this);
//    menuBar()->addMenu (imageMenu);
    
    channelMenu = new QMenu(tr("Channels"));
//    viewChannelFullButton = new QRadioButton(tr("Full Color"));
//    channelMenu-L
    channelMenu->addAction (viewChannelFullAct);
    channelMenu->addAction (viewChannelRedAct);
    channelMenu->addAction (viewChannelGreenAct);
    channelMenu->addAction (viewChannelBlueAct);
    channelMenu->addAction (viewChannelAlphaAct);
    channelMenu->addAction (viewChannelLuminanceAct);
    channelMenu->addAction (viewChannelPrevAct);
    channelMenu->addAction (viewChannelNextAct);

    viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction (prevImageAct);
    viewMenu->addAction (nextImageAct);
    viewMenu->addSeparator ();
    viewMenu->addAction (zoomInAct);
    viewMenu->addAction (zoomOutAct);
    viewMenu->addAction (normalSizeAct);
    viewMenu->addAction (fitToWindowAct);
    viewMenu->addSeparator ();
    viewMenu->addMenu (channelMenu);
    viewMenu->addMenu (expgamMenu);
    menuBar()->addMenu (viewMenu);
    // Fit image to window
    // Fit window to image
    // Full screen mode
    // Channel views: full color C, red R, green G, blue B, alpha A, luminance L
    // Color mode: true, random, falsegrgbacCrgR
    // prev channel ',', next channel '.'
    // prev subimage <, next subimage >
    // fg/bg image...

    toolsMenu = new QMenu(tr("&Tools"), this);
    menuBar()->addMenu (toolsMenu);
    // Mode: select, zoom, pan, wipe
    // Pixel view
    // Image info
    // Menus, toolbars, & status
    // Annotate
    // [check] overwrite render
    // connect renderer
    // kill renderer
    // store render
    // Playback: forward, reverse, faster, slower, loop/pingpong

    helpMenu = new QMenu(tr("&Help"), this);
    helpMenu->addAction (aboutAct);
    menuBar()->addMenu (helpMenu);
    // Bring up user's guide
}



void
ImageViewer::createToolBars()
{
#if 0
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(newAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(saveAct);

    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(cutAct);
    editToolBar->addAction(copyAct);
    editToolBar->addAction(pasteAct);
#endif
}



void
ImageViewer::createStatusBar()
{
    statusImgInfo = new QLabel;
    statusBar()->addWidget (statusImgInfo);

    statusViewInfo = new QLabel;
    statusBar()->addWidget (statusViewInfo);

    statusProgress = new QProgressBar;
    statusProgress->setRange (0, 100);
    statusProgress->reset ();
    statusBar()->addWidget (statusProgress);
}



void
ImageViewer::readSettings()
{
//    QSettings settings("OpenImgageIO", "iv");
//    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
//    QSize size = settings.value("size", QSize(400, 400)).toSize();
//    move(pos);
//    resize(size);
}



void
ImageViewer::writeSettings()
{
//    QSettings settings("OpenImageIO", "iv");
//    settings.setValue("pos", pos());
//    settings.setValue("size", size());
}



bool
image_progress_callback (void *opaque, float done)
{
    ImageViewer *viewer = (ImageViewer *) opaque;
    viewer->statusProgress->setValue ((int)(done*100));
    return false;
}



void ImageViewer::open()
{
    QString qfileName = QFileDialog::getOpenFileName (this,
                                    tr("Open File"), QDir::currentPath());
    std::string filename = qfileName.toStdString();
    if (filename.empty())
        return;

    add_image (filename, false);
    m_current_image = m_images.size()-1;
    IvImage *newimage = m_images[m_current_image];
    newimage->read (false, image_progress_callback, this);
    displayCurrentImage ();
}



void ImageViewer::reload()
{
    if (m_images.empty())
        return;
    IvImage *newimage = m_images[m_current_image];
    newimage->read (true, image_progress_callback, this);
    displayCurrentImage ();
}



void
ImageViewer::add_image (const std::string &filename, bool getspec)
{
    if (filename.empty())
        return;
    IvImage *newimage = new IvImage(filename);
    ASSERT (newimage);
    if (getspec) {
        if (! newimage->init_spec (filename)) {
            QMessageBox::information (this, tr("iv Image Viewer"),
                              tr("%1").arg(newimage->error_message().c_str()));
        } else {
            std::cerr << "Added image " << filename << ": " << newimage->spec().width << " x " << newimage->spec().height << "\n";
        }
    }
    m_images.push_back (newimage);
    displayCurrentImage ();
}



void
ImageViewer::displayCurrentImage ()
{
    if (m_images.empty())
        return;
    if (m_current_image < 0 || m_current_image >= (int)m_images.size())
        m_current_image = 0;
    IvImage *img = m_images[m_current_image];
    const ImageIOFormatSpec &spec (img->spec());

    std::string message;
    message = Strutil::format ("iv Image Viewer   %d    %s", m_current_image,
                               img->name().c_str());
    setWindowTitle (message.c_str());

    message = Strutil::format (/*"%d) <b>%s</b> : " */  "%d x %d",
                               /*m_current_image+1, img->name().c_str(), */
                               spec.width, spec.height);
    if (spec.depth > 1)
        message += Strutil::format (" x %d", spec.depth);
    message += Strutil::format (", %d channel %s (%.2f MB)",
                                spec.nchannels,
                                ParamBaseTypeNameString(spec.format),
                                (float)spec.image_bytes() / (1024.0*1024.0));
    statusImgInfo->setText(message.c_str()); // tr("iv status"));
    message = Strutil::format ("%d:%d  exp %+.1f  gam %.2f",
                               1, 1 /* FIXME! */,
                               img->exposure(), img->gamma());
    statusViewInfo->setText(message.c_str()); // tr("iv status"));

    if (! img->read (false, image_progress_callback, this))
        std::cerr << "read failed in displayCurrentImage: " << img->error_message() << "\n";
    QImage qimage (spec.width, spec.height, QImage::Format_ARGB32_Premultiplied);
    const int as = OpenImageIO::AutoStride;
    float gain = powf (2.0, img->exposure());
    float invgamma = 1.0f / img->gamma();
    for (int y = 0;  y < spec.height;  ++y) {
        unsigned char *sl = qimage.scanLine (y);
        unsigned long *argb = (unsigned long *) sl;
        for (int x = 0;  x < spec.width;  ++x)
            argb[x] = 0xff000000;
        // FIXME -- Ugh, Qt's Pixmap stores "ARGB" as a uint32, but
        // because of byte order on Intel chips, it's really BGRA in
        // memory.  Grrr... So we have to move each channel individually.
        if (m_current_channel == viewFullColor) {
            convert_image (1, spec.width, 1, 1,
                           img->scanline (y), spec.format, spec.nchannels, as, as,
                           sl+2, PT_UINT8, 4, as, as, gain, invgamma);
            if (spec.nchannels > 1)
                convert_image (1, spec.width, 1, 1,
                               (char *)img->scanline (y) + 1*spec.channel_bytes(),
                               spec.format, spec.nchannels, as, as,
                               sl+1, PT_UINT8, 4, as, as, gain, invgamma);
            if (spec.nchannels > 2)
                convert_image (1, spec.width, 1, 1,
                               (char *)img->scanline (y) + 2*spec.channel_bytes(),
                               spec.format, spec.nchannels, as, as,
                               sl+0, PT_UINT8, 4, as, as, gain, invgamma);
            if (spec.nchannels > 3)
                convert_image (1, spec.width, 1, 1,
                               (char *)img->scanline (y) + (spec.nchannels-1)*spec.channel_bytes(),
                               spec.format, spec.nchannels, as, as,
                               sl+3, PT_UINT8, 4, as, as);
        } else if (m_current_channel == viewLuminance) {
        } else if (m_current_channel < spec.nchannels) {
            for (int c = 0;  c < 3;  ++c)
                convert_image (1, spec.width, 1, 1,
                               (char *)img->scanline (y) + m_current_channel*spec.channel_bytes(),
                               spec.format, spec.nchannels, as, as,
                               sl+c, PT_UINT8, 4, as, as, gain, invgamma);
            if (spec.nchannels > 3)
                convert_image (1, spec.width, 1, 1,
                               (char *)img->scanline (y) + 
                               (spec.nchannels-1)*spec.channel_bytes(),
                               spec.format, spec.nchannels, as, as,
                               sl+3, PT_UINT8, 4, as, as);
        }
    }


    imageLabel->setPixmap(QPixmap::fromImage(qimage));
    scaleFactor = 1.0;
    
    printAct->setEnabled(true);
    fitToWindowAct->setEnabled(true);
    updateActions();
    
    if (!fitToWindowAct->isChecked())
        imageLabel->adjustSize();

}



void
ImageViewer::prevImage ()
{
    if (m_images.empty())
        return;
    --m_current_image;
    if (m_current_image < 0)
        m_current_image = ((int)m_images.size()) - 1;
    displayCurrentImage ();
}


void
ImageViewer::nextImage ()
{
    if (m_images.empty())
        return;
    ++m_current_image;
    if (m_current_image >= (int)m_images.size())
        m_current_image = 0;
    displayCurrentImage ();
}



void
ImageViewer::exposureMinusOneTenthStop ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->exposure (img->exposure() - 0.1);
    displayCurrentImage();
}


void
ImageViewer::exposureMinusOneHalfStop ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->exposure (img->exposure() - 0.5);
    displayCurrentImage();
}


void
ImageViewer::exposurePlusOneTenthStop ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->exposure (img->exposure() + 0.1);
    displayCurrentImage();
}


void
ImageViewer::exposurePlusOneHalfStop ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->exposure (img->exposure() + 0.5);
    displayCurrentImage();
}



void
ImageViewer::gammaMinus ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->gamma (img->gamma() - 0.05);
    displayCurrentImage();
}


void
ImageViewer::gammaPlus ()
{
    if (m_images.empty())
        return;
    IvImage *img = m_images[m_current_image];
    img->gamma (img->gamma() + 0.05);
    displayCurrentImage();
}



void
ImageViewer::viewChannelFull ()
{
    m_current_channel = viewFullColor;
    displayCurrentImage();
}


void
ImageViewer::viewChannelRed ()
{
    m_current_channel = viewR;
    displayCurrentImage();
}


void
ImageViewer::viewChannelGreen ()
{
    m_current_channel = viewG;
    displayCurrentImage();
}


void
ImageViewer::viewChannelBlue ()
{
    m_current_channel = viewB;
    displayCurrentImage();
}


void
ImageViewer::viewChannelAlpha ()
{
    m_current_channel = viewA;
    displayCurrentImage();
}


void
ImageViewer::viewChannelLuminance ()
{
    m_current_channel = viewLuminance;
    displayCurrentImage();
}


void
ImageViewer::viewChannelPrev ()
{
    if (m_current_channel)
        --m_current_channel;
    displayCurrentImage();
}


void
ImageViewer::viewChannelNext ()
{
    ++m_current_channel;
    displayCurrentImage();
}



void
ImageViewer::keyPressEvent (QKeyEvent *event)
{
    switch (event->key()) {
#if 0
    case Qt::Key_ParenLeft :
        gammaMinus();
        break;
    case Qt::Key_ParenRight :
        gammaPlus();
        break;
#endif
    default:
//        std::cerr << "ImageViewer key " << (int)event->key() << '\n';
        QMainWindow::keyPressEvent (event);
    }
}



void
ImageViewer::closeImg()
{
    if (m_images.empty())
        return;
    delete m_images[m_current_image];
    m_images[m_current_image] = NULL;
    m_images.erase (m_images.begin()+m_current_image);

    // FIXME:
    // For all image indices we may be storing,
    //   if == m_current_image, wrap to 0 if this was the last image
    //   else if > m_current_image, subtract one

    if (m_current_image >= m_images.size())
        m_current_image = 0;

    displayCurrentImage();
}



void
ImageViewer::print()
{
    Q_ASSERT(imageLabel->pixmap());
#ifndef QT_NO_PRINTER
    QPrintDialog dialog(&printer, this);
    if (dialog.exec()) {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = imageLabel->pixmap()->size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(imageLabel->pixmap()->rect());
        painter.drawPixmap(0, 0, *imageLabel->pixmap());
    }
#endif
}



void ImageViewer::zoomIn()
{
    scaleImage(1.25);
}

void ImageViewer::zoomOut()
{
    scaleImage(0.8);
}


void ImageViewer::normalSize()
{
    imageLabel->adjustSize();
    scaleFactor = 1.0;
}



void ImageViewer::fitToWindow()
{
    bool fitToWindow = fitToWindowAct->isChecked();
    scrollArea->setWidgetResizable(fitToWindow);
    if (!fitToWindow) {
        normalSize();
    }
    updateActions();
}



void ImageViewer::about()
{
    QMessageBox::about(this, tr("About iv"),
            tr("<p><b>iv</b> is the image viewer for OpenImageIO.</p>"
               "<p>(c) Copyright 2008 Larry Gritz.  All Rights Reserved.</p>"
               "<p>See URL-GOES-HERE for details.</p>"));
}


void ImageViewer::updateActions()
{
    zoomInAct->setEnabled(!fitToWindowAct->isChecked());
    zoomOutAct->setEnabled(!fitToWindowAct->isChecked());
    normalSizeAct->setEnabled(!fitToWindowAct->isChecked());
}



void ImageViewer::scaleImage(double factor)
{
    Q_ASSERT(imageLabel->pixmap());
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap()->size());

    adjustScrollBar(scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(scrollArea->verticalScrollBar(), factor);

    zoomInAct->setEnabled(scaleFactor < 3.0);
    zoomOutAct->setEnabled(scaleFactor > 0.333);
}


void ImageViewer::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value()
                            + ((factor - 1) * scrollBar->pageStep()/2)));
}

