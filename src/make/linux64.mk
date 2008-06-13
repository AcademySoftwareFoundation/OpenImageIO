# License and copyright goes here


# This file contains the Linux-specific macros
#
# This is included by platform.mk



CFLAGS += -DLINUX -DLINUX64 
# 64-bit Linux should compile using PIC code
CFLAGS += -fPIC

CP := cp -uvpf

QT_INCLUDE := -I/usr/include/qt4/QtGui -I/usr/include/qt4/QtOpenGL \
	      -I/usr/include/qt4
LINK_QT := -lQtOpenGL -lQtGui -lQtCore 

OPENGL_INCLUDE := -I/usr/include/GL
LINK_OPENGL := 

LINK_OTHER := -ldl

