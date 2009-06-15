# License and copyright goes here

# Name of the binary or library whose source is in this directory.
# Do NOT include .exe or any other suffix.
local_name := targa.imageio

# Name of all source files in this directory
local_src := targainput.cpp targaoutput.cpp

# Extra static libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_libs := 

# Extra shared libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_shlibs := libOpenImageIO

# ld flags needed for this library
#local_ldflags := ${LINK_PNG} #${PNG_HOME}/lib/png-${PNG_VERSION}/*${OEXT}
#local_ldflags :=${LINK_PNG} ${JPEG_HOME}/lib/jpeg-${JPEG_VERSION}/*${OEXT}
local_ldflags :=



## Include ONE of the includes below, depending on whether this module
## constructs a binary executable, static library, shared library (DLL),
## or plugin..

#include ${src_make_dir}/bin.mk
#include ${src_make_dir}/lib.mk
include ${src_make_dir}/shlib.mk
#include ${src_make_dir}/plugin.mk



local_shlibs :=
