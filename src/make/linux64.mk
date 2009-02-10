# License and copyright goes here


# This file contains the Linux-specific macros
#
# This is included by platform.mk



CFLAGS += -DLINUX -DLINUX64 
# 64-bit Linux should compile using PIC code
CFLAGS += -fPIC

ifdef DEBUG
CFLAGS += -g
else
CFLAGS += -O3 -funroll-loops -DNDEBUG
endif

ifdef PROFILE
CFLAGS += -pg
# also -g?
LDFLAGS += -pg
STRIP_BINARY := touch
endif

CP := cp -uvpf

QT_PREFIX ?= /usr/include/qt4
QT_INCLUDE ?= -I${QT_PREFIX}/QtGui -I${QT_PREFIX}/QtOpenGL \
	      -I${QT_PREFIX}
LINK_QT ?= -lQtOpenGL -lQtGui -lQtCore 

OPENGL_INCLUDE ?= -I/usr/include/GL
LINK_OPENGL ?= 

LINK_OTHER := -ldl -lpthread

