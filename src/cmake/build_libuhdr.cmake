# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# libuhdr by hand!
######################################################################

set_cache (libuhdr_BUILD_VERSION 1.4.0 "libultrahdr version for local builds")
set (libuhdr_GIT_REPOSITORY "https://github.com/google/libultrahdr")
set (libuhdr_GIT_TAG "v${libuhdr_BUILD_VERSION}")

set_cache (libuhdr_BUILD_SHARED_LIBS OFF
           DOC "Should execute a local libuhdr build, if necessary, build shared libraries" ADVANCED)

if (TARGET libjpeg-turbo::jpeg)
    # We've had some trouble with libuhdr finding the JPEG resources it needs to
    # build if we're using libjpeg-turbo, libuhdr needs an extra nudge.
    get_target_property(JPEG_INCLUDE_DIR JPEG::JPEG INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(JPEG_LIBRARY JPEG::JPEG INTERFACE_LINK_LIBRARIES)
endif ()

set_cache (UHDR_CMAKE_C_COMPILER ${CMAKE_C_COMPILER} "libuhdr build C compiler override" ADVANCED)
set_cache (UHDR_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} "libuhdr build C++ compiler override" ADVANCED)

build_dependency_with_cmake(libuhdr
    VERSION         ${libuhdr_BUILD_VERSION}
    GIT_REPOSITORY  ${libuhdr_GIT_REPOSITORY}
    GIT_TAG         ${libuhdr_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${libuhdr_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D UHDR_BUILD_EXAMPLES=FALSE
        -D UHDR_BUILD_DEPS=FALSE
        -D UHDR_ENABLE_LOGS=TRUE
        -D JPEG_INCLUDE_DIR=${JPEG_INCLUDE_DIR}
        -D JPEG_LIBRARY=${JPEG_LIBRARY}
        -D CMAKE_C_COMPILER=${UHDR_CMAKE_C_COMPILER}
        -D CMAKE_CXX_COMPILER=${UHDR_CMAKE_CXX_COMPILER}
    )

if (WIN32)
    file (GLOB _lib_files "${libuhdr_LOCAL_BUILD_DIR}/Release/*.lib")
    file (COPY ${_lib_files} DESTINATION ${libuhdr_LOCAL_INSTALL_DIR}/lib)
    unset (_lib_files)
    file (GLOB _header_files "${libuhdr_LOCAL_SOURCE_DIR}/ultrahdr_api.h")
    file (COPY ${_header_files} DESTINATION ${libuhdr_LOCAL_INSTALL_DIR}/include)
    unset (_header_files)
endif ()

set (libuhdr_ROOT ${libuhdr_LOCAL_INSTALL_DIR})

find_package(libuhdr REQUIRED)

set (libuhdr_VERSION ${libuhdr_BUILD_VERSION})

if (libuhdr_BUILD_SHARED_LIBS)
    install_local_dependency_libs (uhdr uhdr)
endif ()
