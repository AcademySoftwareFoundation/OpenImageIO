# License and copyright goes here

#########################################################################
# project.mk
#
# This is where we put all project-specific make commands, including 
# the list of files that becomes our public distribution
#########################################################################


# dist_files lists (relative to build) all files that end up in an
# external distribution
#dist_bins    	:= d${BINEXT}
dist_libs     	:= libimageio${SHLIBEXT} \
		   tiff.imageio${SHLIBEXT}
dist_includes	:= export.h imageio.h paramtype.h


# Path for including things specific to this project

ifeq (${THIRD_PARTY_TOOLS_HOME},)
  THIRD_PARTY_TOOLS_HOME := ../3party/dist/${platform}
endif

ILMBASE_VERSION := 1.0.1
ifeq (${ILMBASE_HOME},)
  ILMBASE_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
ILMBASE_CXX := -I${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR
LINK_ILMBASE := ${LD_LIBPATH}${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION} -lHalf

OPENEXR_VERSION := 1.6.1
ifeq (${OPENEXR_HOME},)
  OPENEXR_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
OPENEXR_CXX := -I${OPENEXR_HOME}/include/OpenEXR-${OPENEXR_VERSION}/OpenEXR
LINK_OPENEXR := ${LD_LIBPATH}${OPENEXR_HOME}/lib/OpenEXR-${OPENEXR_VERSION} -lIlmImf

TIFF_VERSION := 3.8.2
ifeq (${TIFF_HOME},)
  TIFF_HOME := ${THIRD_PARTY_TOOLS_HOME}
endif
TIFF_CXX := -I${TIFF_HOME}/include/tiff-${TIFF_VERSION}
LINK_TIFF := ${LD_LIBPATH}${TIFF_HOME}/lib/tiff-${TIFF_VERSION} -ltiff


PROJECT_EXTRA_CXX := ${ILMBASE_CXX} ${OPENEXR_CXX} ${TIFF_CXX}

