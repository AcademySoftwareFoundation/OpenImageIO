# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# PNG by hand!
######################################################################

set_cache (PNG_BUILD_VERSION 1.6.47 "PNG version for local builds")
super_set (PNG_BUILD_GIT_REPOSITORY "https://github.com/pnggroup/libpng")
super_set (PNG_BUILD_GIT_TAG "v${PNG_BUILD_VERSION}")
super_set (PNG_BUILD_EXTRA_CMAKE_ARGS "")
set_cache (PNG_BUILD_SHARED_LIBS OFF
           DOC "Should execute a local PNG build, if necessary, build shared libraries" ADVANCED)
set_option (PNG_BUILD_USE_CUSTOM_PREFIX "Use custom namespace prefix for libpng" OFF)

if (PNG_BUILD_USE_CUSTOM_PREFIX)
    list (APPEND PNG_BUILD_EXTRA_CMAKE_ARGS -D PNG_PREFIX=oiio)
endif ()
string (MAKE_C_IDENTIFIER ${PNG_BUILD_VERSION} PNG_VERSION_IDENT)

unset (PNG_FOUND)
unset (PNG_LIBRARY)
unset (PNG_LIBRARY_RELEASE)
unset (PNG_LIBRARY_DEBUG)
unset (PNG_LIBRARIES)
unset (PNG_INCLUDE_DIRS)
unset (PNG_INCLUDE_DIR)
unset (PNG_PNG_INCLUDE_DIR)
unset (PNG_VERSION_STRING)
unset (PNG_DEFINITIONS)
unset (PNG_VERSION)

build_dependency_with_cmake (PNG
    VERSION         ${PNG_BUILD_VERSION}
    GIT_REPOSITORY  ${PNG_BUILD_GIT_REPOSITORY}
    GIT_TAG         ${PNG_BUILD_GIT_TAG}
    CMAKE_ARGS
        -D PNG_SHARED=${PNG_BUILD_SHARED_LIBS}
        -D PNG_STATIC=ON
        -D PNG_EXECUTABLES=OFF
        -D PNG_TESTS=OFF
        -D PNG_TOOLS=OFF
        -D PNG_FRAMEWORK=OFF
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        ${PNG_BUILD_EXTRA_CMAKE_ARGS}
    )

    
set (PNG_REFIND FALSE)
set (PNG_REFIND_VERSION ${PNG_BUILD_VERSION})
set (PNG_REFIND_ARGS EXACT REQUIRED)
set (PNG_FIND_VERSION_EXACT ON)
unset (PNG_FOUND)
unset (PNG_LIBRARY)
unset (PNG_LIBRARY_RELEASE)
unset (PNG_LIBRARY_DEBUG)
unset (PNG_LIBRARIES)
unset (PNG_INCLUDE_DIRS)
unset (PNG_INCLUDE_DIR)
unset (PNG_PNG_INCLUDE_DIR)
unset (PNG_VERSION_STRING)
unset (PNG_DEFINITIONS)
unset (PNG_VERSION)

if (PNG_BUILD_VERSION VERSION_GREATER 1.6.43)
    list (APPEND PNG_REFIND_ARGS CONFIG)
endif ()

find_package(PNG ${PNG_REFIND_VERSION} ${PNG_REFIND_ARGS}
             HINTS 
                    ${PNG_LOCAL_INSTALL_DIR}/lib/cmake/PNG
                    ${PNG_LOCAL_INSTALL_DIR}
             NO_DEFAULT_PATH
            )

set (PNG_INCLUDE_DIRS ${PNG_LOCAL_INSTALL_DIR}/include)
include_directories(BEFORE ${PNG_INCLUDE_DIRS})

if (PNG_BUILD_SHARED_LIBS)
    install_local_dependency_libs (PNG png)
endif ()
