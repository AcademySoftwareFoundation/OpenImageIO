# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# PNG by hand!
######################################################################

set_cache (PNG_BUILD_VERSION 1.6.44 "PNG version for local builds")
set (PNG_GIT_REPOSITORY "https://github.com/glennrp/libpng")
set (PNG_GIT_TAG "v${PNG_BUILD_VERSION}")

set_cache (PNG_BUILD_SHARED_LIBS OFF #${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should execute a local PNG build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${PNG_BUILD_VERSION} PNG_VERSION_IDENT)

build_dependency_with_cmake(PNG
    VERSION         ${PNG_BUILD_VERSION}
    GIT_REPOSITORY  ${PNG_GIT_REPOSITORY}
    GIT_TAG         ${PNG_GIT_TAG}
    CMAKE_ARGS
        -D PNG_SHARED=${PNG_BUILD_SHARED_LIBS}
        -D PNG_EXECUTABLES=OFF
        -D PNG_TESTS=OFF
        -D PNG_FRAMEWORK=OFF
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
    )

# Set some things up that we'll need for a subsequent find_package to work
set (PNG_ROOT ${PNG_LOCAL_INSTALL_DIR})
set (PNG_DIR ${PNG_LOCAL_INSTALL_DIR})
# Signal to caller that we need to find again at the installed location
set (PNG_REFIND TRUE)
set (PNG_REFIND_VERSION ${PNG_BUILD_VERSION})
if ({$PNG_BUILD_VERSION} VERSION_GREATER_EQUAL "1.6.44")
    # CMake configs added in libpng 1.6.44
    set (PNG_REFIND_ARGS CONFIG)  
endif ()

if (PNG_BUILD_SHARED_LIBS)
    install_local_dependency_libs (PNG PNG)
endif ()
