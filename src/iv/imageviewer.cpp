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
#include <unistd.h>

#include <ImathFun.h>

#include <boost/foreach.hpp>
#include <ImathFun.h>

#include "imageviewer.h"
#include "dassert.h"
#include "strutil.h"
#include "timer.h"




/// Subclass QScrollArea just so we can intercept events that we want
/// handled in non-default ways.
class IvCanvas : public QScrollArea
{
public:
    IvCanvas (ImageViewer &viewer) : m_viewer(viewer), m_dragging(false) { }
private:
    void keyPressEvent (QKeyEvent *event);
    void mousePressEvent (QMouseEvent *event);
    void mouseReleaseEvent (QMouseEvent *event);
    void mouseMoveEvent (QMouseEvent *event);

    typedef QScrollArea parent_t;
    ImageViewer &m_viewer;
    bool m_dragging;                  ///< Are we dragging?
    int m_drag_oldx, m_drag_oldy;
    Qt::MouseButton m_drag_button;    ///< Button on when dragging
};



/// Replacement keyPressEvent for IvScroll Area intercepts the arrows
/// that we want to page between images in our UI, not to be hooked to
/// the scroll bars.
void
IvCanvas::keyPressEvent (QKeyEvent *event)
{
//    std::cerr << "IvCanvas key " << (int)event->key() << '\n';
    switch (event->key()) {
#if 1
    case Qt::Key_Left :
    case Qt::Key_Up :
    case Qt::Key_PageUp :
        m_viewer.prevImage();
        return;  //break;
    case Qt::Key_Right :
//        std::cerr << "Modifier is " << (int)event->modifiers() << '\n';
//        fprintf (stderr, "%x\n", (int)event->modifiers());
//        if (event->modifiers() & Qt::ShiftModifier)
//            std::cerr << "hey, ctrl right\n";
    case Qt::Key_Down :
    case Qt::Key_PageDown :
        m_viewer.nextImage();
        return; //break;
    case Qt::Key_Escape :
        if (m_viewer.m_fullscreen)
            m_viewer.fullScreenToggle();
        return;
#endif
    }
    parent_t::keyPressEvent (event);
}



void
IvCanvas::mousePressEvent (QMouseEvent *event)
{
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
        m_drag_oldx = event->x();
        m_drag_oldy = event->y();
        std::cerr << "Middle press\n";
        break;
    }
    parent_t::mousePressEvent (event);
}



void
IvCanvas::mouseReleaseEvent (QMouseEvent *event)
{
    m_drag_button = Qt::NoButton;
    switch (event->button()) {
    case Qt::MidButton :
        m_dragging = false;
        std::cerr << "Middle release\n";
        break;
    }
    parent_t::mouseReleaseEvent (event);
}



void
IvCanvas::mouseMoveEvent (QMouseEvent *event)
{
    // FIXME - there's probably a better Qt way than tracking the button
    // myself.
    switch (m_drag_button /*event->button()*/) {
    case Qt::MidButton : {
        QPoint pos = event->pos(); //, oldpos = event->oldPos();
        std::cerr << "Middle drag " << m_drag_oldx << ',' << m_drag_oldy 
                  << " -> " << pos.x() << ',' << pos.y() << '\n';
        break;
        }
    }
    parent_t::mouseMoveEvent (event);
}




ImageViewer::ImageViewer ()
    : infoWindow(NULL),
      m_current_image(-1), m_current_channel(-1), m_last_image(-1),
      m_zoom(1), m_fullscreen(false)
{
    scrollArea = new IvCanvas (*this);
    scrollArea->setBackgroundRole (QPalette::Dark);
    scrollArea->setAlignment (Qt::AlignCenter);
    glwin = new IvGL (this, this);
    glwin->resize (640, 480);
    scrollArea->setWidget (glwin);
    setCentralWidget (scrollArea);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    setWindowTitle (tr("Image Viewer"));
    resize (640, 480);
//    setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
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

    reloadAct = new QAction(tr("&Reload image"), this);
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
    viewChannelFullAct->setCheckable (true);
    viewChannelFullAct->setChecked (true);
    connect(viewChannelFullAct, SIGNAL(triggered()), this, SLOT(viewChannelFull()));

    viewChannelRedAct = new QAction(tr("Red"), this);
    viewChannelRedAct->setShortcut(tr("r"));
    viewChannelRedAct->setCheckable (true);
    connect(viewChannelRedAct, SIGNAL(triggered()), this, SLOT(viewChannelRed()));

    viewChannelGreenAct = new QAction(tr("Green"), this);
    viewChannelGreenAct->setShortcut(tr("g"));
    viewChannelGreenAct->setCheckable (true);
    connect(viewChannelGreenAct, SIGNAL(triggered()), this, SLOT(viewChannelGreen()));

    viewChannelBlueAct = new QAction(tr("Blue"), this);
    viewChannelBlueAct->setShortcut(tr("b"));
    viewChannelBlueAct->setCheckable (true);
    connect(viewChannelBlueAct, SIGNAL(triggered()), this, SLOT(viewChannelBlue()));

    viewChannelAlphaAct = new QAction(tr("Alpha"), this);
    viewChannelAlphaAct->setShortcut(tr("a"));
    viewChannelAlphaAct->setCheckable (true);
    connect(viewChannelAlphaAct, SIGNAL(triggered()), this, SLOT(viewChannelAlpha()));

    viewChannelLuminanceAct = new QAction(tr("Luminance"), this);
    viewChannelLuminanceAct->setShortcut(tr("l"));
    viewChannelLuminanceAct->setCheckable (true);
    connect(viewChannelLuminanceAct, SIGNAL(triggered()), this, SLOT(viewChannelLuminance()));

    viewChannelPrevAct = new QAction(tr("Prev Channel"), this);
    viewChannelPrevAct->setShortcut(tr(","));
    connect(viewChannelPrevAct, SIGNAL(triggered()), this, SLOT(viewChannelPrev()));

    viewChannelNextAct = new QAction(tr("Next Channel"), this);
    viewChannelNextAct->setShortcut(tr("."));
    connect(viewChannelNextAct, SIGNAL(triggered()), this, SLOT(viewChannelNext()));

    zoomInAct = new QAction(tr("Zoom &In"), this);
    zoomInAct->setShortcut(tr("Ctrl++"));
    zoomInAct->setEnabled(false);
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomOutAct = new QAction(tr("Zoom &Out"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    zoomOutAct->setEnabled(false);
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

    normalSizeAct = new QAction(tr("&Normal Size (1:1)"), this);
    normalSizeAct->setShortcut(tr("Ctrl+0"));
    normalSizeAct->setEnabled(false);
    connect(normalSizeAct, SIGNAL(triggered()), this, SLOT(normalSize()));

    fitWindowToImageAct = new QAction(tr("&Fit Window to Image"), this);
    fitWindowToImageAct->setEnabled(false);
//    fitWindowToImageAct->setCheckable(true);
    fitWindowToImageAct->setShortcut(tr("f"));
    connect(fitWindowToImageAct, SIGNAL(triggered()), this, SLOT(fitWindowToImage()));

    fitImageToWindowAct = new QAction(tr("Fit Image to Window"), this);
    fitImageToWindowAct->setEnabled(false);
//    fitImageToWindowAct->setCheckable(true);
    fitImageToWindowAct->setShortcut(tr("Alt+f"));
    connect(fitImageToWindowAct, SIGNAL(triggered()), this, SLOT(fitImageToWindow()));

    fullScreenAct = new QAction(tr("Full screen"), this);
    fullScreenAct->setEnabled(false);
//    fullScreenAct->setCheckable(true);
    fullScreenAct->setShortcut(tr("Ctrl+f"));
    connect(fullScreenAct, SIGNAL(triggered()), this, SLOT(fullScreenToggle()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    prevImageAct = new QAction(tr("Previous Image"), this);
    prevImageAct->setShortcut(tr("PgUp"));  // FIXME: Does this work?
    prevImageAct->setEnabled(true);
    connect (prevImageAct, SIGNAL(triggered()), this, SLOT(prevImage()));

    nextImageAct = new QAction(tr("Next Image"), this);
    nextImageAct->setShortcut(tr("PageDown"));
    nextImageAct->setEnabled(true);
    connect (nextImageAct, SIGNAL(triggered()), this, SLOT(nextImage()));

    toggleImageAct = new QAction(tr("Toggle image"), this);
    toggleImageAct->setShortcut(tr("t"));
    toggleImageAct->setEnabled(true);
    connect (toggleImageAct, SIGNAL(triggered()), this, SLOT(toggleImage()));

    showInfoWindowAct = new QAction(tr("&Image info..."), this);
    showInfoWindowAct->setShortcut(tr("Ctrl+I"));
//    showInfoWindowAct->setEnabled(true);
    connect (showInfoWindowAct, SIGNAL(triggered()), this, SLOT(showInfoWindow()));
}



void
ImageViewer::createMenus()
{
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction (openAct);
    fileMenu->addAction (reloadAct);
    fileMenu->addAction (closeImgAct);
    fileMenu->addSeparator ();
    // Save as ^S
    // Save window as Shift-Ctrl-S
    // Save selection as
    fileMenu->addSeparator ();
    fileMenu->addAction (printAct);
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
    // Color mode: true, random, falsegrgbacCrgR
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
    viewMenu->addAction (toggleImageAct);
    viewMenu->addSeparator ();
    viewMenu->addAction (zoomInAct);
    viewMenu->addAction (zoomOutAct);
    viewMenu->addAction (normalSizeAct);
    viewMenu->addAction (fitWindowToImageAct);
    viewMenu->addAction (fitImageToWindowAct);
    viewMenu->addAction (fullScreenAct);
    viewMenu->addSeparator ();
    viewMenu->addMenu (channelMenu);
    viewMenu->addMenu (expgamMenu);
    menuBar()->addMenu (viewMenu);
    // Full screen mode
    // prev subimage <, next subimage >
    // fg/bg image...

    toolsMenu = new QMenu(tr("&Tools"), this);
    // Mode: select, zoom, pan, wipe
    // Pixel view
    toolsMenu->addAction (showInfoWindowAct);
    // Menus, toolbars, & status
    // Annotate
    // [check] overwrite render
    // connect renderer
    // kill renderer
    // store render
    // Playback: forward, reverse, faster, slower, loop/pingpong
    menuBar()->addMenu (toolsMenu);

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
    QApplication::processEvents();
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
    int n = m_images.size()-1;
    IvImage *newimage = m_images[n];
    newimage->read (false, image_progress_callback, this);
    current_image (n);
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
ImageViewer::updateTitle ()
{
    IvImage *img = cur();
    if (! img)
        return;
    std::string message;
    message = Strutil::format ("%s - iv Image Viewer", img->name().c_str());
    setWindowTitle (message.c_str());
}



void
ImageViewer::updateStatusBar ()
{
    const ImageIOFormatSpec *spec = curspec();
    if (! spec)
        return;
    std::string message;
    message = Strutil::format ("%d/%d) : ", m_current_image+1, m_images.size());
    message += cur()->shortinfo();
    statusImgInfo->setText (message.c_str());

    switch (m_current_channel) {
    case channelFullColor: message = "RGB"; break;
    case channelLuminance: message = "Lum"; break;
    default:
        if (spec->channelnames.size() > m_current_channel &&
                spec->channelnames[m_current_channel].size())
            message = spec->channelnames[m_current_channel];
        else
            message = Strutil::format ("chan %d", m_current_channel);
        break;
    }
    message += Strutil::format ("  %g:%g  exp %+.1f  gam %.2f",
                                zoom() >= 1 ? zoom() : 1.0f,
                                zoom() >= 1 ? 1.0f : 1.0f/zoom(),
                                cur()->exposure(), cur()->gamma());
    statusViewInfo->setText(message.c_str()); // tr("iv status"));
}



void
ImageViewer::displayCurrentImage ()
{
    if (m_images.empty()) {
        m_current_image = m_last_image = -1;
        return;
    }
    if (m_current_image < 0 || m_current_image >= (int)m_images.size())
        m_current_image = 0;
    IvImage *img = cur();
    const ImageIOFormatSpec &spec (img->spec());

    if (! img->read (false, image_progress_callback, this))
        std::cerr << "read failed in displayCurrentImage: " << img->error_message() << "\n";

    updateTitle();
    updateStatusBar();
    if (infoWindow)
        infoWindow->update (img);

    glwin->zoom (zoom());
    glwin->update (img);

    printAct->setEnabled(true);
    fitWindowToImageAct->setEnabled(true);
    fitImageToWindowAct->setEnabled(true);
    fullScreenAct->setEnabled(true);
    updateActions();
}



void
ImageViewer::current_image (int newimage)
{
    if (m_images.empty() || newimage < 0 || newimage >= (int)m_images.size())
        return;
    if (m_current_image != newimage) {
        m_last_image = (m_current_image >= 0) ? m_current_image : newimage;
        m_current_image = newimage;
        displayCurrentImage ();
    }
}



void
ImageViewer::prevImage ()
{
    if (m_images.empty())
        return;
    if (m_current_image == 0)
        current_image ((int)m_images.size() - 1);
    else
        current_image (current_image() - 1);
}


void
ImageViewer::nextImage ()
{
    if (m_images.empty())
        return;
    if (m_current_image >= (int)m_images.size()-1)
        current_image (0);
    else
        current_image (current_image() + 1);
}



void
ImageViewer::toggleImage ()
{
    current_image (m_last_image);
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
ImageViewer::viewChannel (ChannelView c)
{
    if (m_current_channel != c) {
        m_current_channel = c;
        displayCurrentImage();
        viewChannelFullAct->setChecked (c == channelFullColor);
        viewChannelRedAct->setChecked (c == channelRed);
        viewChannelGreenAct->setChecked (c == channelGreen);
        viewChannelBlueAct->setChecked (c == channelBlue);
        viewChannelAlphaAct->setChecked (c == channelAlpha);
        viewChannelLuminanceAct->setChecked (c == channelLuminance);
    }
}



void
ImageViewer::viewChannelFull ()
{
    viewChannel (channelFullColor);
}


void
ImageViewer::viewChannelRed ()
{
    viewChannel (channelRed);
}


void
ImageViewer::viewChannelGreen ()
{
    viewChannel (channelGreen);
}


void
ImageViewer::viewChannelBlue ()
{
    viewChannel (channelBlue);
}


void
ImageViewer::viewChannelAlpha ()
{
    viewChannel (channelAlpha);
}


void
ImageViewer::viewChannelLuminance ()
{
    viewChannel (channelLuminance);
}


void
ImageViewer::viewChannelPrev ()
{
    if ((int)m_current_channel >= 0)
        viewChannel ((ChannelView)((int)m_current_channel - 1));
}


void
ImageViewer::viewChannelNext ()
{
    viewChannel ((ChannelView)((int)m_current_channel + 1));
}



void
ImageViewer::keyPressEvent (QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Minus :
    case Qt::Key_Underscore :
        zoomOut();
        break;
    case Qt::Key_Plus :
    case Qt::Key_Equal :
        zoomIn();
        break;
    default:
        std::cerr << "ImageViewer key " << (int)event->key() << '\n';
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

    current_image (current_image() < m_images.size() ? current_image() : 0);
}



void
ImageViewer::print()
{
#if 0
    Q_ASSERT(imageLabel->pixmap());
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
    if (zoom() >= 64)
        return;
    if (zoom() >= 1.0f) {
        int z = (int) zoom();
        zoom ((float)(z + 1));
    } else {
        int z = (int)(1.0 / zoom());
        zoom (1.0f / std::max(z-1,1));
    }
}



void ImageViewer::zoomOut()
{
    if (zoom() <= 1.0f/64)
        return;
    if (zoom() > 1.0f) {
        int z = (int) zoom();
        zoom (std::max ((float)(z-1), 0.5f));
    } else {
        int z = (int)(1.0 / zoom() + 0.001);  // add for floating point slop
        zoom (1.0f / (1 + z));
    }
}


void ImageViewer::normalSize()
{
    zoom (1.0f);
}



void ImageViewer::fitImageToWindow()
{
    IvImage *img = cur();
    if (! img)
        return;
    const ImageIOFormatSpec &spec (img->spec());
    std::cerr << "fitImageToWindow wh = " << scrollArea->width() << ' ' << scrollArea->height() << "\n";
    QSize s = scrollArea->maximumViewportSize();
    std::cerr << "  max " << s.width() << ' ' << s.height() << "\n";
    float zw = (float) s.width() / spec.width;
    float zh = (float) s.height() / spec.height;
    std::cerr << "zoom potentials " << zw << ' ' << zh << "\n";
    float z = std::min (zw, zh);
    zoom (z);
}



void ImageViewer::fitWindowToImage()
{
    IvImage *img = cur();
    if (! img)
        return;
    // FIXME -- figure out a way to make it exactly right, even for the
    // main window border, etc.
    int extraw = 12; // width() - minimumWidth();
    int extrah = 40; // height() - minimumHeight();
//    std::cerr << "extra wh = " << extraw << ' ' << extrah << '\n';
//    scrollArea->resize ((int)(img->spec().width * zoom()),
//                        (int)(img->spec().height * zoom()));
    resize ((int)(img->spec().width * zoom())+extraw,
            (int)(img->spec().height * zoom())+extrah);
    zoom (zoom());

#if 0
    bool fit = fitWindowToImageAct->isChecked();
    scrollArea->setWidgetResizable(fit);
    if (!fit) {
        normalSize();
    }
#endif
    updateActions();
}



void ImageViewer::fullScreenToggle()
{
    std::cerr << "toggle full screen\n";
    if (m_fullscreen) {
        menuBar()->show ();
        statusBar()->show ();
        showNormal ();
        // glwin->showNormal ();
        // glwin->setParent (scrollArea);
        // scrollArea->setWidget (glwin);
        m_fullscreen = false;
    } else {
        menuBar()->hide ();
        statusBar()->hide ();
        showFullScreen ();
        fitImageToWindow ();
        // glwin->setParent (NULL);  // Make it into a top-level window
        // glwin->showFullScreen ();
        m_fullscreen = true;
    }
}



void
ImageViewer::about()
{
    QMessageBox::about(this, tr("About iv"),
            tr("<p><b>iv</b> is the image viewer for OpenImageIO.</p>"
               "<p>(c) Copyright 2008 Larry Gritz.  All Rights Reserved.</p>"
               "<p>See URL-GOES-HERE for details.</p>"));
}


void ImageViewer::updateActions()
{
    zoomInAct->setEnabled(!fitImageToWindowAct->isChecked());
    zoomOutAct->setEnabled(!fitImageToWindowAct->isChecked());
    normalSizeAct->setEnabled(!fitImageToWindowAct->isChecked());
}



void
ImageViewer::zoom (float newzoom)
{
    IvImage *img = cur();
    if (! img)
        return;
    QScrollBar *hsb = scrollArea->horizontalScrollBar();
    QScrollBar *vsb = scrollArea->verticalScrollBar();

    // Zoom so that the center of the viewport stays on the same pixel
    int centerh, centerv, oldcenterh, oldcenterv;
    QSize viewsize = scrollArea->maximumViewportSize();
    oldcenterh = Imath::clamp ((int)((viewsize.width()/2 + hsb->value())/zoom()), 0, curspec()->width-1);
    oldcenterv = Imath::clamp ((int)((viewsize.height()/2 + vsb->value())/zoom()), 0, curspec()->height-1);

    float oldzoom = m_zoom;
    const int nsteps = 10;
    for (int i = 1;  i <= nsteps;  ++i) {
        float z = Imath::lerp (oldzoom, newzoom, (float)i/(float)nsteps);
        m_zoom = z;

        glwin->zoom (zoom());

        centerh = (int)(zoom() * oldcenterh) - viewsize.width()/2;
        centerv = (int)(zoom() * oldcenterv) - viewsize.height()/2;
        hsb->setValue (centerh);
        vsb->setValue (centerv);
//        QApplication::processEvents();
        glwin->trigger_redraw();
        if (i != nsteps)
            usleep (1000000 / 4 / nsteps);
    }

    zoomInAct->setEnabled (zoom() < 64.0);
    zoomOutAct->setEnabled (zoom() > 1.0/64);

    updateStatusBar ();
}



void
ImageViewer::showInfoWindow ()
{
    if (! infoWindow) {
        std::cerr << "Making new info window\n";
        infoWindow = new IvInfoWindow (this, true);
    }
    infoWindow->update (cur());
    infoWindow->show();
}
