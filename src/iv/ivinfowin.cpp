// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <iostream>

#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "imageviewer.h"
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/strutil.h>



IvInfoWindow::IvInfoWindow(ImageViewer& viewer, bool visible)
    : QDialog(&viewer)
    , m_viewer(viewer)
    , m_visible(visible)
{
    infoLabel = new QLabel;
    infoLabel->setPalette(viewer.palette());

    scrollArea = new QScrollArea;
    scrollArea->setPalette(viewer.palette());
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(infoLabel);
    scrollArea->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                          QSizePolicy::MinimumExpanding,
                                          QSizePolicy::Label));
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setAlignment(Qt::AlignTop);

    closeButton = new QPushButton(tr("Close"));
    connect(closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(scrollArea);
    mainLayout->addWidget(closeButton);
    setLayout(mainLayout);
    infoLabel->show();
    scrollArea->show();

    setWindowTitle(tr("Image Info"));
}



void
IvInfoWindow::update(IvImage* img)
{
    std::string newtitle;
    if (img) {
        newtitle = Strutil::sprintf("%s - iv Info", img->name().c_str());
        infoLabel->setText(img->longinfo().c_str());
    } else {
        newtitle = Strutil::sprintf("iv Info");
        infoLabel->setText(tr("No image loaded."));
    }
    setWindowTitle(newtitle.c_str());
}



void
IvInfoWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_W
        && (event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        hide();
    } else {
        event->ignore();
    }
}
