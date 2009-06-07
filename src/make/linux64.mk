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

QT_INCLUDE_DIR ?= ${shell qmake -query QT_INSTALL_HEADERS}
QT_LIBRARY_DIR ?= ${shell qmake -query QT_INSTALL_LIBS}

ifeq (${QT_INCLUDE_DIR},)
    $(info )
    $(info *** Qt not found!)
    $(info )
    USE_QT := 0
endif

ifneq (${USE_QT},0)
QT_INCLUDE ?= -I${QT_INCLUDE_DIR}/QtGui -I${QT_INCLUDE_DIR}/QtOpenGL \
             -I${QT_INCLUDE_DIR}
LINK_QT ?= -L${QT_LIBRARY_DIR} -lQtOpenGL -lQtGui -lQtCore 
endif

ifneq (${USE_OPENGL},0)
OPENGL_INCLUDE ?= -I/usr/include/GL
LINK_OPENGL ?= 
endif

LINK_OTHER := -ldl -lpthread

