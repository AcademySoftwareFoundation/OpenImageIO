# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# Freetype by hand!
######################################################################

set_cache (Freetype_BUILD_VERSION 2.14.1 "Freetype version for local builds")
set (Freetype_GIT_REPOSITORY "https://github.com/freetype/freetype")
set (Freetype_GIT_TAG "VER-2-14-1")
set_cache (Freetype_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local Freetype build, if necessary, build shared libraries" ADVANCED)
# We would prefer to build a static Freetype, but haven't figured out how to make
# it all work with the static dependencies, it just makes things complicated
# downstream.

string (MAKE_C_IDENTIFIER ${Freetype_BUILD_VERSION} Freetype_VERSION_IDENT)

# Conditionally disable support for PNG-compressed OpenType embedded bitmaps on MacOS
# https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4423#issuecomment-2455217897
if ( APPLE )
    set (_freetype_EXTRA_CMAKE_ARGS -DFT_DISABLE_PNG=ON )
endif ()

build_dependency_with_cmake(Freetype
    VERSION         ${Freetype_BUILD_VERSION}
    GIT_REPOSITORY  ${Freetype_GIT_REPOSITORY}
    GIT_TAG         ${Freetype_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${Freetype_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        ${_freetype_EXTRA_CMAKE_ARGS}
        # Fix for freetype breaking against cmake 4.0.
        # Remove when freetype is fixed to declare its own minimum high enough.
        -D CMAKE_POLICY_VERSION_MINIMUM=3.5
)

# Set some things up that we'll need for a subsequent find_package to work

set (Freetype_ROOT ${Freetype_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (Freetype_REFIND TRUE)
set (Freetype_REFIND_ARGS CONFIG)

if (Freetype_BUILD_SHARED_LIBS)
    install_local_dependency_libs (Freetype Freetype)
endif ()
