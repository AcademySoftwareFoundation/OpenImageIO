# License and copyright goes here


# This file contains the Mac OS X specific macros
#
# This is included by platform.mk



CFLAGS += -fPIC
#CFLAGS += -D__APPLE__    # the compiler already does this for us
#CFLAGS += -arch i386 -arch x86_64 -mmacosx-version-min=10.5

# I don't understand why, but boost::regex crashes when I strip binaries on OSX.
STRIP_BINARY := touch

ifdef DEBUG
CFLAGS += -g -W
else
CFLAGS += -O3 -funroll-loops -DNDEBUG
#helpful? -funroll-loops -fomit-frame-pointer 
#unhelpful? -march=pentium4 -ffast-math -msse -mfpmath=sse -msse2
endif

ifdef PROFILE
# On OS X, use "Shark", not gprof, so don't bother with -pg
#CFLAGS += -pg
#LDFLAGS += -pg
CFLAGS += -g
STRIP_BINARY := touch
endif

# ? -fno-common

#LDFLAGS += -arch x86_64
SHLIBEXT := .dylib
SHLIB_LDFLAGS := -dynamiclib 
#SHLIB_LDFLAGS += -arch x86_64
# -m64

QT_INCLUDE += -I/Library/Frameworks/QtGui.framework/Headers
QT_INCLUDE += -I/Library/Frameworks/QtOpenGL.framework/Headers
LINK_QT += -framework QtGui -framework QtOpenGL -framework QtCore

OPENGL_INCLUDE := 
LINK_OPENGL := -framework OpenGL
