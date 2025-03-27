# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# libdeflate by hand!
######################################################################

set_cache (libdeflate_BUILD_VERSION 1.23 "libdeflate version for local builds")
set (libdeflate_GIT_REPOSITORY "https://github.com/ebiggers/libdeflate")
set (libdeflate_GIT_TAG "v${libdeflate_BUILD_VERSION}")
set_cache (libdeflate_BUILD_SHARED_LIBS OFF # ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local libdeflate build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${libdeflate_BUILD_VERSION} libdeflate_VERSION_IDENT)

build_dependency_with_cmake(libdeflate
    VERSION         ${libdeflate_BUILD_VERSION}
    GIT_REPOSITORY  ${libdeflate_GIT_REPOSITORY}
    GIT_TAG         ${libdeflate_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${libdeflate_BUILD_SHARED_LIBS}
        -D LIBDEFLATE_BUILD_SHARED_LIB=${libdeflate_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        -D LIBDEFLATE_BUILD_GZIP=OFF
    )

# Set some things up that we'll need for a subsequent find_package to work

set (libdeflate_ROOT ${libdeflate_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (libdeflate_REFIND TRUE)
set (libdeflate_REFIND_ARGS CONFIG)
set (libdeflate_REFIND_VERSION ${libdeflate_BUILD_VERSION})

if (libdeflate_BUILD_SHARED_LIBS)
    install_local_dependency_libs (libdeflate libdeflate)
endif ()
