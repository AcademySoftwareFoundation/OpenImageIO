# License and copyright goes here


# This file contains the Mac OS X specific macros
#
# This is included by platform.mk



CFLAGS += -DMACOSX
#CFLAGS += -arch i386 -arch x86_64 -mmacosx-version-min=10.5

ifdef DEBUG
CFLAGS += -g
else
CFLAGS += -O3 
endif

# ? -fno-common

#LDFLAGS += -arch x86_64
SHLIBEXT := .dylib
SHLIB_LDFLAGS := -dynamiclib 
#SHLIB_LDFLAGS += -arch x86_64
# -m64

QT_INCLUDE := -I/Library/Frameworks/QtGui.framework/Headers
QT_INCLUDE += -I/Library/Frameworks/QtOpenGL.framework/Headers
#QT_INCLUDE := -FQtGui.framework
#QT_INCLUDE += -FQtOpenGL.framework
LINK_QT := /Library/Frameworks/QtGui.framework/QtGui
LINK_QT += /Library/Frameworks/QtOpenGL.framework/QtOpenGL
LINK_QT += /Library/Frameworks/QtCore.framework/QtCore

OPENGL_INCLUDE := 
LINK_OPENGL := -L/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/ -lGL
