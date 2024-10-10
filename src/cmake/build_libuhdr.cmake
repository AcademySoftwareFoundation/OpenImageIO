# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# libuhdr by hand!
######################################################################

set_cache (libuhdr_BUILD_VERSION 1.2.0 "libuhdr version for local builds")
set (libuhdr_GIT_REPOSITORY "https://github.com/google/libultrahdr")
set (libuhdr_GIT_TAG "v${libuhdr_BUILD_VERSION}")

set_cache (libuhdr_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should execute a local libuhdr build, if necessary, build shared libraries" ADVANCED)

build_dependency_with_cmake(libuhdr
    VERSION         ${libuhdr_BUILD_VERSION}
    GIT_REPOSITORY  ${libuhdr_GIT_REPOSITORY}
    GIT_TAG         ${libuhdr_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${libdeflate_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
    )

set (libuhdr_ROOT ${libuhdr_LOCAL_INSTALL_DIR})

find_package(libuhdr REQUIRED)

set (libuhdr_VERSION ${libuhdr_BUILD_VERSION})

if (libuhdr_BUILD_SHARED_LIBS)
    install_local_dependency_libs (uhdr uhdr)
endif ()
