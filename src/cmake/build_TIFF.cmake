# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# TIFF by hand!
######################################################################

set_cache (TIFF_BUILD_VERSION 4.6.0 "TIFF version for local builds")
set (TIFF_GIT_REPOSITORY "https://gitlab.com/libtiff/libtiff.git")
set (TIFF_GIT_TAG "v${TIFF_BUILD_VERSION}")
set_cache (TIFF_BUILD_SHARED_LIBS  ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local TIFF build, if necessary, build shared libraries" ADVANCED)

# We need libdeflate to build libtiff
checked_find_package (libdeflate REQUIRED
                      VERSION_MIN 1.18)
alias_library_if_not_exists (Deflate::Deflate libdeflate::libdeflate_static)

if (TARGET libjpeg-turbo::jpeg)
    # We've had some trouble with TIFF finding the JPEG resources it needs to
    # build if we're using libjpeg-turbo, TIFF needs an extra nudge.
    get_target_property(JPEG_INCLUDE_DIRS JPEG::JPEG INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(JPEG_LIBRARIES JPEG::JPEG INTERFACE_LINK_LIBRARIES)
    set (JPEG_FOUND TRUE)
    set (MORE_TIFF_CMAKE_ARGS
         -D JPEG_INCLUDE_DIR=${JPEG_INCLUDE_DIRS}
         -D JPEG_LIBRARY=${JPEG_LIBRARIES} )
endif ()

build_dependency_with_cmake(TIFF
    VERSION         ${TIFF_BUILD_VERSION}
    GIT_REPOSITORY  ${TIFF_GIT_REPOSITORY}
    GIT_TAG         ${TIFF_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${TIFF_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        -D tiff-tools=OFF
        -D tiff-contrib=OFF
        -D tiff-tests=OFF
        -D tiff-docs=OFF
        -D libdeflate=ON
        -D lzma=OFF
        -D zstd=OFF
        -D jbig=OFF
        ${MORE_TIFF_CMAKE_ARGS}
    )

# Set some things up that we'll need for a subsequent find_package to work

set (TIFF_ROOT ${TIFF_LOCAL_INSTALL_DIR})
set (TIFF_DIR ${TIFF_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
# set (TIFF_REFIND TRUE)
find_package (TIFF REQUIRED)

if (TIFF_BUILD_SHARED_LIBS)
    install_local_dependency_libs (TIFF TIFF)
endif ()

unset (MORE_TIFF_CMAKE_ARGS)
