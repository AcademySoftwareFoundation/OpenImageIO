# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# GIF by hand!
######################################################################

set_cache (GIF_BUILD_VERSION 5.2.1 "GIFLIB version for local builds")
set (GIF_URL "https://downloads.sourceforge.net/project/giflib/giflib-${GIF_BUILD_VERSION}.tar.gz")
set (GIF_URL_HASH SHA256=31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd)

set_cache (GIF_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should execute a local GIF build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${GIF_BUILD_VERSION} GIF_VERSION_IDENT)

# Build setup using ExternalProject
build_dependency_with_cmake(GIF
    VERSION         ${GIF_BUILD_VERSION}
    URL             ${GIF_URL}
    URL_HASH        ${GIF_URL_HASH}
    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=${GIF_LOCAL_INSTALL_DIR}
        -D BUILD_SHARED_LIBS=${GIF_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
    PATCH_COMMAND
        curl -L https://sourceforge.net/p/giflib/bugs/_discuss/thread/4e811ad29b/c323/attachment/Makefile.patch -o Makefile.patch &&
        patch -p0 < Makefile.patch
)

# Set up paths for find_package
set (GIF_ROOT ${GIF_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (GIF_REFIND TRUE)
set (GIF_REFIND_VERSION ${GIF_BUILD_VERSION})
set (GIF_REFIND_ARGS CONFIG)

if (GIF_BUILD_SHARED_LIBS)
    install_local_dependency_libs (GIF GIF)
endif ()

