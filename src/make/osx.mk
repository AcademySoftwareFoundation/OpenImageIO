# License and copyright goes here


# This file contains the Mac OS X specific macros
#
# This is included by platform.mk



CFLAGS += -DMACOSX -DOSX
# ? -fno-common

SHLIBEXT := .dylib
SHLIB_LDFLAGS := -dynamiclib

QT_INCLUDE := -I/Library/Frameworks/QtGui.framework/Headers
LINK_QT := /Library/Frameworks/QtGui.framework/QtGui
LINK_QT += /Library/Frameworks/QtOpenGL.framework/QtOpenGL
LINK_QT += /Library/Frameworks/QtCore.framework/QtCore


