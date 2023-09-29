// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <iostream>

#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "imageviewer.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>



IvPreferenceWindow::IvPreferenceWindow(ImageViewer& viewer)
    : QDialog(&viewer)
    , m_viewer(viewer)
{
    closeButton = new QPushButton(tr("Close"));
    closeButton->setShortcut(tr("Ctrl+W"));
    connect(closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    layout = new QVBoxLayout;
    layout->addWidget(viewer.pixelviewFollowsMouseBox);
    layout->addWidget(viewer.linearInterpolationBox);
    layout->addWidget(viewer.darkPaletteBox);
    layout->addWidget(viewer.autoMipmap);

    QLayout* inner_layout = new QHBoxLayout;
    inner_layout->addWidget(viewer.maxMemoryICLabel);
    inner_layout->addWidget(viewer.maxMemoryIC);

    QLayout* slideShowLayout = new QHBoxLayout;
    slideShowLayout->addWidget(viewer.slideShowDurationLabel);
    slideShowLayout->addWidget(viewer.slideShowDuration);

    layout->addLayout(inner_layout);
    layout->addLayout(slideShowLayout);
    layout->addWidget(closeButton);
    setLayout(layout);

    setWindowTitle(tr("iv Preferences"));
}



void
IvPreferenceWindow::keyPressEvent(QKeyEvent* event)
{
    //    if (event->key() == 'w' && (event->modifiers() & Qt::ControlModifier)) {
    if (event->key() == Qt::Key_W) {
        std::cerr << "found w\n";
        if ((event->modifiers() & Qt::ControlModifier)) {
            std::cerr << "think we did ctrl-w\n";
            event->accept();
            hide();
        } else
            std::cerr << "modifier " << (int)event->modifiers() << "\n";
    } else {
        event->ignore();
    }
}
