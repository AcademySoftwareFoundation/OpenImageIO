# License and copyright goes here

#########################################################################
# project.mk
#
# This is where we put all project-specific make commands, including 
# the list of files that becomes our public distribution
#########################################################################


all:

#########################################################################
# 'make help' prints important make targets
help:
	@echo "Targets:"
	@echo "  make            (default target is 'all')"
	@echo "  make all        Build optimized binaries and libraries in ${dist_dir},"
	@echo "                      temporary build files in ${build_dir}"
	@echo "  make debug      Build unoptimized with symbols in ${dist_dir}.debug,"
	@echo "                      temporary build files in ${build_dir}.debug"
	@echo "  make profile    Build for profiling in ${dist_dir}.profile,"
	@echo "                      temporary build files in ${build_dir}.profile"
	@echo "  make clean      Remove the temporary files in ${build_dir}"
	@echo "  make realclean  Remove both ${build_dir} AND ${dist_dir}"
	@echo "  make nuke       Remove ALL of build and dist (not just ${platform})"
	@echo "Helpful modifiers:"
	@echo "  make EMBEDPLUGINS=1 ...     compile the plugins into libimageio"
	@echo ""



#########################################################################
# dist_files lists (relative to build) all files that end up in an
# external distribution
dist_bins    	:= iconvert${BINEXT} \
		   idiff${BINEXT} \
		   iinfo${BINEXT} \
		   iv${BINEXT} \
		   maketx${BINEXT} \
		   testtex${BINEXT}
dist_libs     	:= libimageio${SHLIBEXT}
#		   libtexture${SHLIBEXT}

# Only dist the plugins if we're building without embedded plugins
ifeq (${EMBEDPLUGINS},)
dist_libs       += hdr.imageio${SHLIBEXT} \
		   jpeg.imageio${SHLIBEXT} \
		   openexr.imageio${SHLIBEXT} \
		   png.imageio${SHLIBEXT} \
		   tiff.imageio${SHLIBEXT}
endif

# include files that get included in the compiled distribution
dist_includes	:= export.h imageio.h typedesc.h imagebuf.h paramlist.h \
			texture.h typedesc.h ustring.h varyingref.h

# make the public distro have an OpenImageIO subdirectory in include,
# to avoid name clashes
dist_include_dir := include/OpenImageIO

# docs files that get copied to dist
dist_docs	:= src/doc/CLA-INDIVIDUAL src/doc/CLA-CORPORATE \
		   src/doc/openimageio.pdf

# files at the root level that get copied to dist
dist_root	:= LICENSE INSTALL CHANGES



#########################################################################
# Project-specific make variables

ifneq (${EMBEDPLUGINS},)
  CFLAGS += -DEMBED_PLUGINS
endif



#########################################################################
# Path for including things specific to this project

ifeq (${THIRD_PARTY_TOOLS_HOME},)
  THIRD_PARTY_TOOLS_HOME := ../3party/dist/${platform}
endif

ILMBASE_VERSION := 1.0.1
ifeq (${ILMBASE_HOME},)
  ILMBASE_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
ILMBASE_LIB_AREA := ${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION}
ILMBASE_CXX := -I${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR
LINK_ILMBASE_HALF := ${ILMBASE_LIB_AREA}/half${OEXT}
LINK_ILMBASE := ${LD_LIBPATH}${ILMBASE_LIB_AREA} -lImath ${LINK_ILMBASE_HALF} -lIlmThread -lIex

OPENEXR_VERSION := 1.6.1
ifeq (${OPENEXR_HOME},)
  OPENEXR_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
OPENEXR_CXX := -I${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION}/OpenEXR -DOPENEXR_VERSION=010400
OPENEXR_LIB_AREA := ${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION}
LINK_OPENEXR := ${LD_LIBPATH}${OPENEXR_LIB_AREA} -lIlmImf

TIFF_VERSION := 3.8.2
ifeq (${TIFF_HOME},)
  TIFF_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
TIFF_CXX := -I${TIFF_HOME}/include/tiff-${TIFF_VERSION}
TIFF_LIB_AREA := ${TIFF_HOME}/lib/tiff-${TIFF_VERSION}
LINK_TIFF := ${LD_LIBPATH}${TIFF_LIB_AREA} -ltiff

JPEG_VERSION := 6b
ifeq (${JPEG_HOME},)
  JPEG_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
JPEG_CXX := -I${JPEG_HOME}/include/jpeg-${JPEG_VERSION}
LINK_JPEG := ${LD_LIBPATH}${JPEG_HOME}/lib/jpeg-${JPEG_VERSION} -ljpeg

PNG_VERSION := 1.2.31
ifeq (${PNG_HOME},)
  PNG_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
PNG_CXX := -I${PNG_HOME}/include/libpng-${PNG_VERSION}
LINK_PNG := ${LD_LIBPATH}${PNG_HOME}/lib/libpng-${PNG_VERSION} -lpng

ZLIB_VERSION := 1.2.3
ifeq (${ZLIB_HOME},)
  ZLIB_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
ZLIB_CXX := -I${ZLIB_HOME}/include/zlib-${ZLIB_VERSION}
LINK_ZLIB := ${LD_LIBPATH}${ZLIB_HOME}/lib/zlib-${ZLIB_VERSION} -lz

BOOST_VERSION := 1_35_0
ifeq (${BOOST_HOME},)
  BOOST_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
BOOST_CXX := -I${BOOST_HOME}/include/boost_${BOOST_VERSION}
BOOST_LIB_AREA := ${BOOST_HOME}/lib/boost_${BOOST_VERSION}
ifeq (${platform},macosx)
  BOOST_SUFFIX := -mt-1_35
else
  GCCVERCODE := ${shell gcc --version | grep -o "[0-9]\.[0-9]" | head -1 | tr -d "."}
  BOOST_SUFFIX := -gcc${GCCVERCODE}-mt-1_35
endif

LINK_BOOST := ${LD_LIBPATH}${BOOST_LIB_AREA}
LINK_BOOST += -lboost_filesystem${BOOST_SUFFIX} \
	      -lboost_system${BOOST_SUFFIX} \
	      -lboost_thread${BOOST_SUFFIX}

TBB_VERSION := 21_20080605oss
ifeq (${TBB_HOME},)
  TBB_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
TBB_CXX := -I${TBB_HOME}/include/tbb${TBB_VERSION}
TBB_LIB_AREA := ${TBB_HOME}/lib/tbb${TBB_VERSION}
LINK_TBB := ${LD_LIBPATH}${TBB_LIB_AREA} -ltbb


dist_extra_libs += $(wildcard ${BOOST_LIB_AREA}/libboost_filesystem${BOOST_SUFFIX}${SHLIBEXT}*) \
		   $(wildcard ${BOOST_LIB_AREA}/libboost_system${BOOST_SUFFIX}${SHLIBEXT}*) \
		   $(wildcard ${BOOST_LIB_AREA}/libboost_thread${BOOST_SUFFIX}${SHLIBEXT}*) \
		   ${ILMBASE_LIB_AREA}/libHalf${SHLIBEXT} \
		   $(wildcard ${ILMBASE_LIB_AREA}/libImath${SHLIBEXT}*) \
		   $(wildcard ${ILMBASE_LIB_AREA}/libIex${SHLIBEXT}*) \
		   $(wildcard ${ILMBASE_LIB_AREA}/libIlmThread${SHLIBEXT}*)


PROJECT_EXTRA_CXX := ${ILMBASE_CXX} ${OPENEXR_CXX} ${TIFF_CXX} ${JPEG_CXX} \
			${PNG_CXX} ${ZLIB_CXX} ${BOOST_CXX} ${TBB_CXX}

PROJECT_EXTRA_CXX += ${QT_INCLUDE} ${OPENGL_INCLUDE}


