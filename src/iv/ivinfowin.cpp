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


#include <iostream>

#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "imageviewer.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>



IvInfoWindow::IvInfoWindow (ImageViewer &viewer, bool visible)
    : QDialog(&viewer), m_viewer(viewer), m_visible (visible)
{
    infoLabel = new QLabel;
    infoLabel->setPalette (viewer.palette());

    scrollArea = new QScrollArea;
    scrollArea->setPalette (viewer.palette());
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
IvInfoWindow::update (IvImage *img)
{
    std::string newtitle;
    if (img) {
        newtitle = Strutil::format ("%s - iv Info", img->name().c_str());
        infoLabel->setText (img->longinfo().c_str());
    } else {
        newtitle = Strutil::format ("iv Info");
        infoLabel->setText (tr("No image loaded."));
    }
    setWindowTitle (newtitle.c_str());
}



void
IvInfoWindow::keyPressEvent (QKeyEvent *event)
{
    if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        hide();
    } else {
        event->ignore();
    }
}
