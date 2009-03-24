# Template for the makefile for an individual src/* directory.
# Fill in the blanks below.

# License and copyright goes here

# Name of the binary or library whose source is in this directory.
# Do NOT include .exe or any other suffix.
local_name := libOpenImageIO

# Name of all source files in this directory
local_src := formatspec.cpp imageinput.cpp imageio.cpp \
		imageoutput.cpp imageioplugin.cpp \
		imagebuf.cpp imagebufalgo.cpp \
		iptc.cpp xmp.cpp

# Extra static libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_libs := 

# Extra objects from other libs we need to compile this library 
local_extra_objs := ${build_obj_dir}/libutil/argparse${OEXT} \
		    ${build_obj_dir}/libutil/filesystem${OEXT} \
		    ${build_obj_dir}/libutil/filter${OEXT} \
		    ${build_obj_dir}/libutil/paramlist${OEXT} \
		    ${build_obj_dir}/libutil/plugin${OEXT} \
		    ${build_obj_dir}/libutil/strutil${OEXT} \
		    ${build_obj_dir}/libutil/sysutil${OEXT} \
		    ${build_obj_dir}/libutil/typedesc${OEXT} \
		    ${build_obj_dir}/libutil/ustring${OEXT} \
		    ${build_obj_dir}/libtexture/texturesys${OEXT} \
		    ${build_obj_dir}/libtexture/texoptions${OEXT} \
		    ${build_obj_dir}/libtexture/imagecache${OEXT}

# Extra shared libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_shlibs := 

# ld flags needed for this library
ifeq (${platform},macosx)
# Only OSX appears to need to link a dynamic lib against static libs
# that they will call
local_ldflags := ${LINK_ILMBASE} ${LINK_BOOST}
endif


# If somebody did 'make EMBEDPLUGINS=1', take the object files for all
# the standard plugins and build them right into libOpenImageIO, so that the
# DSO/DLL's don't need to be found at runtime.
ifneq (${EMBEDPLUGINS},)
local_extra_objs += \
	${build_obj_dir}/hdr.imageio/hdrinput${OEXT} \
	${build_obj_dir}/hdr.imageio/hdroutput${OEXT} \
	${build_obj_dir}/hdr.imageio/rgbe${OEXT} \
	${build_obj_dir}/jpeg.imageio/jpeginput${OEXT} \
	${build_obj_dir}/jpeg.imageio/jpegoutput${OEXT} \
	${build_obj_dir}/jpeg.imageio/jpegexif${OEXT} \
	${build_obj_dir}/png.imageio/pnginput${OEXT} \
	${build_obj_dir}/png.imageio/pngoutput${OEXT} \
	${build_obj_dir}/openexr.imageio/exrinput${OEXT} \
	${build_obj_dir}/openexr.imageio/exroutput${OEXT} \
	${build_obj_dir}/tiff.imageio/tiffinput${OEXT} \
	${build_obj_dir}/tiff.imageio/tiffoutput${OEXT} \
	${build_obj_dir}/zfile.imageio/zfile${OEXT}
local_ldflags += ${LINK_OPENEXR} ${LINK_TIFF} ${LINK_PNG} \
		${LINK_JPEG} ${LINK_ZLIB} 
ifeq (${BOOST_DYNAMIC},1)
local_ldflags += ${LINK_BOOST}
endif
endif


## Include ONE of the includes below, depending on whether this module
## constructs a binary executable, a static library, or a shared library
## (DLL).

#include ${src_make_dir}/bin.mk
#include ${src_make_dir}/lib.mk
include ${src_make_dir}/shlib.mk
