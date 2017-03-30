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
#include <QSpinBox>
#include <QVBoxLayout>

#include "imageviewer.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>



IvPreferenceWindow::IvPreferenceWindow (ImageViewer &viewer)
    : QDialog(&viewer), m_viewer(viewer)
{
    closeButton = new QPushButton (tr("Close"));
    closeButton->setShortcut (tr("Ctrl+W"));
    connect (closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    layout = new QVBoxLayout;
    layout->addWidget (viewer.pixelviewFollowsMouseBox);
    layout->addWidget (viewer.linearInterpolationBox);
    layout->addWidget (viewer.darkPaletteBox);
    layout->addWidget (viewer.autoMipmap);
    
    QLayout *inner_layout = new QHBoxLayout;
    inner_layout->addWidget (viewer.maxMemoryICLabel);
    inner_layout->addWidget (viewer.maxMemoryIC);

    QLayout *slideShowLayout = new QHBoxLayout;
    slideShowLayout->addWidget (viewer.slideShowDurationLabel);
    slideShowLayout->addWidget (viewer.slideShowDuration);

    layout->addLayout (inner_layout);
    layout->addLayout (slideShowLayout);
    layout->addWidget (closeButton);
    setLayout (layout);

    setWindowTitle (tr("iv Preferences"));
}



void
IvPreferenceWindow::keyPressEvent (QKeyEvent *event)
{
//    if (event->key() == 'w' && (event->modifiers() & Qt::ControlModifier)) {
    if (event->key() == Qt::Key_W) {
        std::cerr << "found w\n";
        if ((event->modifiers() & Qt::ControlModifier)) {
            std::cerr << "think we did ctrl-w\n";
            event->accept();
            hide();
        }
        else std::cerr << "modifier " << (int)event->modifiers() << "\n";
    } else {
        event->ignore();
    }
}
