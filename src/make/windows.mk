# License and copyright goes here


# This file contains the Linux-specific macros
#
# This is included by platform.mk



CFLAGS += -DWINDOWS

ifdef DEBUG
CFLAGS += /Yd -Z7 -GX -MTd /Od /DDEBUG
else
CFLAGS += /Ox /arch:SSE /G6 /Gy /Ob2 /vmb /DNDEBUG
endif

ifdef PROFILE
CFLAGS := /Zi /GR
LDFLAGS += 
STRIP_BINARY := @echo strip
endif

# C and C++ compilation
CXX := cl /nologo
CC := ${CXX}
CFLAGS += /I${src_include_dir}
DASHC := /c #
DASHO := /Fo #
CINCL := -I
OEXT := .obj

# Creating static libraries
LIBPREFIX := lib
LIBEXT := .lib
AR := lib
AROUT := /OUT:
ARPREREQ = $^

# Linking an executable
BINEXT := .exe
LD := link /nologo
BINOUT := /OUT:
LD_LIBPATH := /LIBPATH:
LDFLAGS += 
LINKWITH := -l

# Creating a dynamic/shared library
SHLIBEXT := .dll
LDSHLIB := link /nologo
SHLIB_DASHO := /OUT:
SHLIB_LDFLAGS := 

# Making dependency make files (.d)
MAKEDEPEND := makedepend
DEPENDFLAGS := -Y

# Making compilers
FLEX := flex
FLEXARGS := --nounistd
BISON := bison
BISONARGS :=

# Miscellaneous shell commands
RM := rm
RM_ALL := rm -rf
CHMOD_W := chmod +w
CHMOD_RO := chmod -w
CHMOD_RX := chmod 555
STRIP_BINARY := strip
MKDIR := mkdir -p
CP := cp -vpf
CPR := cp -vpfr
SED := sed


QT_INCLUDE := -I/usr/include/qt4/QtGui -I/usr/include/qt4/QtOpenGL \
	      -I/usr/include/qt4
LINK_QT := -lQtOpenGL -lQtGui -lQtCore 

OPENGL_INCLUDE := -I/usr/include/GL
LINK_OPENGL := opengl32.lib glu32.lib

LINK_OTHER := pthreadVCE.lib kernel32.lib user32.lib gdi32.lib 

