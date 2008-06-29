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



IvPixelviewWindow::IvPixelviewWindow (ImageViewer &viewer, bool visible)
    : QDialog(&viewer), m_viewer(viewer), m_visible (visible)
{
    infoLabel = new QLabel;

    closeup = new IvGLPixelview (viewer);
    closeup->resize (100, 100);
//    closeup->setSizePolicy (QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored));

    closeButton = new QPushButton (tr("Close"));
    connect (closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget (closeup);
    mainLayout->addWidget (infoLabel);
    closeup->show();
    mainLayout->addWidget (closeButton);
    setLayout (mainLayout);

    closeup->setFixedHeight (sizeHint().height());
    closeup->setFixedWidth (sizeHint().width());

    setWindowTitle (tr("iv Pixel View"));
}



void
IvPixelviewWindow::update (IvImage *img)
{
//    IvImage *img = m_viewer.cur();
    if (! img)
        return;
    const ImageIOFormatSpec &spec (img->spec());

    std::string newtitle = Strutil::format ("%s - iv Pixel View", img->name().c_str());
    setWindowTitle (newtitle.c_str());
    if (img) {
        std::string s;
        int x, y;
        char *pixel = (char *) alloca (spec.pixel_bytes());
        float *fpixel = (float *) alloca (spec.nchannels*sizeof(float));
        m_viewer.glwin->get_focus_pixel (x, y);
        if (x >= 0 && x <= spec.width && y >= 0 && y <= spec.height) {
            s += Strutil::format ("<p>(%d, %d)</p>", x+spec.x, y+spec.y);
            s += "<table>";
            img->getpixel (x, y, fpixel);
            if (spec.format == PT_UINT8) {
                unsigned char *p = (unsigned char *) img->pixeladdr (x, y);
                for (int i = 0;  i < spec.nchannels;  ++i) {
                    s += html_table_row (spec.channelnames[i].c_str(),
                                         Strutil::format ("%3d  (%5.3f)",
                                                    (int)(p[i]), fpixel[i]));
                }
            } else {
                // Treat as float
                for (int i = 0;  i < spec.nchannels;  ++i) {
                    s += html_table_row (spec.channelnames[i].c_str(),
                                         fpixel[i]);
                }
            }
            s += "</table>";
        }
        infoLabel->setText (s.c_str());
    } else {
        infoLabel->setText (tr("No image loaded."));
    }
//    closeup->resize (100, 100);
//    closeup->setSizePolicy (QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
}



IvGLPixelview::IvGLPixelview (ImageViewer &viewer)
    : m_viewer (viewer)
{
}



IvGLPixelview::~IvGLPixelview ()
{
}



void
IvGLPixelview::initializeGL ()
{
    glClearColor (0.05, 0.05, 0.05, 1.0);
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
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_viewer.glwin->paintGL ();
}



void
IvGLPixelview::useshader ()
{
    m_viewer.glwin->useshader ();
}
