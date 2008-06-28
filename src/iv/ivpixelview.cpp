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

#include "imageviewer.h"
#include "dassert.h"
#include "strutil.h"



IvPixelviewWindow::IvPixelviewWindow (ImageViewer *viewer, bool visible)
    : QDialog(viewer), m_viewer(viewer), m_visible (visible)
{
    ASSERT (viewer != NULL);

    infoLabel = new QLabel;

    scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable (true);
    scrollArea->setWidget (infoLabel);
    scrollArea->setSizePolicy (QSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding, QSizePolicy::Label));
    scrollArea->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameStyle (QFrame::NoFrame);
    scrollArea->setAlignment (Qt::AlignTop);

    closeButton = new QPushButton (tr("Close"));
    connect (closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget (scrollArea);
    mainLayout->addWidget (closeButton);
    setLayout (mainLayout);
    infoLabel->show();
    scrollArea->show();

    setWindowTitle (tr("Image Info"));
}



void
IvPixelviewWindow::update (IvImage *img)
{
//    IvImage *img = m_viewer->cur();
    if (! img)
        return;
    const ImageIOFormatSpec &spec (img->spec());

    std::string newtitle = Strutil::format ("%s - iv Pixel View", img->name().c_str());
    setWindowTitle (newtitle.c_str());
    if (img) {
        std::string s;
        s += "<table>";
        for (int i = 0;  i < spec.nchannels;  ++i) {
            s += html_table_row (spec.channelnames[i].c_str(), 0);
        }
        s += "</table>";
        infoLabel->setText (s.c_str());
    } else {
        infoLabel->setText (tr("No image loaded."));
    }
}



IvGLPixelview::IvGLPixelview (QWidget *parent, ImageViewer &viewer)
    : QGLWidget(parent), m_viewer (viewer)
{
}



IvGLPixelview::~IvGLPixelview ()
{
}



void
IvGLPixelview::initializeGL ()
{
    m_viewer.glwin->initializeGL ();
}



void
IvGLPixelview::resizeGL (int w, int h)
{
    m_viewer.glwin->resizeGL (w, h);
}



void
IvGLPixelview::paintGL ()
{
    m_viewer.glwin->paintGL ();
}



void
IvGLPixelview::useshader ()
{
    m_viewer.glwin->useshader ();
}
