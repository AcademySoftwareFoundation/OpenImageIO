# Template for the makefile for an individual src/* directory.
# Fill in the blanks below.

# License and copyright goes here

# Name of the binary or library whose source is in this directory.
# Do NOT include .exe or any other suffix.
local_name := libimageio

# Name of all source files in this directory
local_src := formatspec.cpp imageinput.cpp imageio.cpp \
		imageoutput.cpp imageioplugin.cpp \
		imagebuf.cpp imagebufalgo.cpp

# Extra static libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_libs := 

# Extra objects from other libs we need to compile this library 
local_extra_objs := ${build_obj_dir}/libutil/argparse${OEXT} \
		    ${build_obj_dir}/libutil/filesystem${OEXT} \
		    ${build_obj_dir}/libutil/paramtype${OEXT} \
		    ${build_obj_dir}/libutil/plugin${OEXT} \
		    ${build_obj_dir}/libutil/strutil${OEXT} \
		    ${build_obj_dir}/libutil/ustring${OEXT}

# Extra shared libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_shlibs := 

# ld flags needed for this library
local_ldflags := ${LINK_ILMBASE} ${LINK_BOOST}


## Include ONE of the includes below, depending on whether this module
## constructs a binary executable, a static library, or a shared library
## (DLL).

#include ${src_make_dir}/bin.mk
#include ${src_make_dir}/lib.mk
include ${src_make_dir}/shlib.mk
