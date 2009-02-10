# License and copyright goes here


# This file contains the Linux-specific macros
#
# This is included by platform.mk



CFLAGS += -DLINUX

ifdef DEBUG
CFLAGS += -g
else
CFLAGS += -O3 -DNDEBUG
endif

ifdef PROFILE
CFLAGS += -pg
# also -g?
LDFLAGS += -pg
STRIP_BINARY := touch
endif

CP := cp -uvpf

QT_INCLUDE ?= -I/usr/include/qt4/QtGui -I/usr/include/qt4/QtOpenGL \
	      -I/usr/include/qt4
LINK_QT ?= -lQtOpenGL -lQtGui -lQtCore 

OPENGL_INCLUDE ?= -I/usr/include/GL
LINK_OPENGL ?= 

LINK_OTHER := -ldl -lpthread

