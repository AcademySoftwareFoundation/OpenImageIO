# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# ZLIB by hand!
######################################################################

set_cache (ZLIB_BUILD_VERSION 1.3.1 "ZLIB version for local builds")
set (ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib")
set (ZLIB_GIT_TAG "v${ZLIB_BUILD_VERSION}")

set_cache (ZLIB_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should execute a local ZLIB build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${ZLIB_BUILD_VERSION} ZLIB_VERSION_IDENT)

build_dependency_with_cmake(ZLIB
    VERSION         ${ZLIB_BUILD_VERSION}
    GIT_REPOSITORY  ${ZLIB_GIT_REPOSITORY}
    GIT_TAG         ${ZLIB_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${ZLIB_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        # Fix for zlib breaking against cmake 4.0.
        # Remove when zlib is fixed to declare its own minimum high enough.
        -D CMAKE_POLICY_VERSION_MINIMUM=3.5
    )

# Set some things up that we'll need for a subsequent find_package to work
set (ZLIB_ROOT ${ZLIB_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (ZLIB_REFIND TRUE)
set (ZLIB_VERSION ${ZLIB_BUILD_VERSION})
set (ZLIB_REFIND_VERSION ${ZLIB_BUILD_VERSION})

if (ZLIB_BUILD_SHARED_LIBS)
    install_local_dependency_libs (ZLIB ZLIB)
endif ()
