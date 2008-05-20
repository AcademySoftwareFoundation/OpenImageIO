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

#include <boost/foreach.hpp>

#include "imageviewer.h"
#include "dassert.h"



ImageViewer::ImageViewer ()
    : m_current_image(-1)
{
    imageLabel = new QLabel;
    imageLabel->setBackgroundRole (QPalette::Base);
    imageLabel->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents (true);

    scrollArea = new QScrollArea;
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

    printAct = new QAction(tr("&Print..."), this);
    printAct->setShortcut(tr("Ctrl+P"));
    printAct->setEnabled(false);
    connect(printAct, SIGNAL(triggered()), this, SLOT(print()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

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
}



void
ImageViewer::createMenus()
{
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addAction(printAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);
    menuBar()->addMenu (fileMenu);

    editMenu = new QMenu(tr("&Edit"), this);
    menuBar()->addMenu (editMenu);

    imageMenu = new QMenu(tr("&Image"), this);
    menuBar()->addMenu (imageMenu);

    viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(normalSizeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(fitToWindowAct);
    menuBar()->addMenu (viewMenu);

    toolsMenu = new QMenu(tr("&Tools"), this);
    menuBar()->addMenu (toolsMenu);

    helpMenu = new QMenu(tr("&Help"), this);
    helpMenu->addAction(aboutAct);
    menuBar()->addMenu (helpMenu);
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
    statusBar()->showMessage(tr("iv status"));
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



void ImageViewer::open()
{
    QString qfileName = QFileDialog::getOpenFileName (this,
                                    tr("Open File"), QDir::currentPath());
    std::string filename = qfileName.toStdString();

    if (filename.empty())
        return;

    IvImage *newimage = new IvImage();
    ASSERT (newimage);
    if (! newimage->read (filename)) {
        QMessageBox::information (this, tr("iv Image Viewer"),
                                  tr("%1").arg(newimage->error_message().c_str()));
    }

    const ImageIOFormatSpec &spec (newimage->spec());
    QImage image (spec.width, spec.height, 
                  QImage::Format_ARGB32_Premultiplied);
    const int as = OpenImageIO::AutoStride;
    for (int y = 0;  y < spec.height;  ++y) {
        unsigned char *sl = image.scanLine (y);
        unsigned long *argb = (unsigned long *) sl;
        for (int x = 0;  x < spec.width;  ++x)
            argb[x] = 0xff000000;
        // FIXME -- Ugh, Qt's Pixmap stores "ARGB" as a uint32, but
        // because of byte order on Intel chips, it's really BGRA in
        // memory.  Grrr... So we have to move each channel individually.
        convert_image (1, spec.width, 1, 1,
                       newimage->scanline (y), spec.format, spec.nchannels, as, as,
                       sl+2, PT_UINT8, 4, as, as);
        if (spec.nchannels > 1)
            convert_image (1, spec.width, 1, 1,
                           (char *)newimage->scanline (y) + 1*spec.channel_bytes(),
                           spec.format, spec.nchannels, as, as,
                           sl+1, PT_UINT8, 4, as, as);
        if (spec.nchannels > 2)
            convert_image (1, spec.width, 1, 1,
                           (char *)newimage->scanline (y) + 2*spec.channel_bytes(),
                           spec.format, spec.nchannels, as, as,
                           sl+0, PT_UINT8, 4, as, as);
        if (spec.nchannels > 3)
            convert_image (1, spec.width, 1, 1,
                           (char *)newimage->scanline (y) + 
                               (spec.nchannels-1)*spec.channel_bytes(),
                           spec.format, spec.nchannels, as, as,
                           sl+3, PT_UINT8, 4, as, as);
    }

    // Add the new image to the image list, and set the current viewing
    // image to the new one.
    m_images.push_back (newimage);
    m_current_image = m_images.size()-1;

    imageLabel->setPixmap(QPixmap::fromImage(image));
    scaleFactor = 1.0;
    
    printAct->setEnabled(true);
    fitToWindowAct->setEnabled(true);
    updateActions();
    
    if (!fitToWindowAct->isChecked())
        imageLabel->adjustSize();
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

