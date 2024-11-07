# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# minizip-ng by hand!
######################################################################


set_cache (minizip-ng_BUILD_VERSION 4.0.7 "minizip-ng version for local builds")
set (minizip-ng_GIT_REPOSITORY "https://github.com/zlib-ng/minizip-ng")
set (minizip-ng_GIT_TAG "${minizip-ng_BUILD_VERSION}") 

set_cache (minizip-ng_BUILD_SHARED_LIBS OFF
           DOC "Should a local minizip-ng build, if necessary, build shared libraries" ADVANCED)

checked_find_package (ZLIB REQUIRED)


build_dependency_with_cmake(minizip-ng
    VERSION         ${minizip-ng_BUILD_VERSION}
    GIT_REPOSITORY  ${minizip-ng_GIT_REPOSITORY}
    GIT_TAG         ${minizip-ng_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${minizip-ng_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        # Since the other modules create a subfolder for the includes by default and since
        # minizip-ng does not, a suffix is added to CMAKE_INSTALL_INCLUDEDIR in order to
        # install the headers under a subdirectory named "minizip-ng".
        # Note that this does not affect external builds for minizip-ng.
        -D CMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}/minizip-ng
        -D MZ_OPENSSL=OFF
        -D MZ_LIBBSD=OFF
        -D MZ_BUILD_TESTS=OFF
        -D MZ_COMPAT=OFF
        -D MZ_BZIP2=OFF
        -D MZ_LZMA=OFF
        -D MZ_LIBCOMP=OFF
        -D MZ_ZSTD=OFF
        -D MZ_PKCRYPT=OFF
        -D MZ_WZAES=OFF
        -D MZ_SIGNING=OFF
        -D MZ_ZLIB=ON
        -D MZ_ICONV=OFF
        -D MZ_FETCH_LIBS=OFF
        -D MZ_FORCE_FETCH_LIBS=OFF
        -D ZLIB_LIBRARY=${ZLIB_LIBRARIES}
        -D ZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIRS}
    )


set (minizip-ng_DIR ${minizip-ng_LOCAL_INSTALL_DIR}/lib/cmake/minizip-ng)
set (minizip-ng_VERSION ${minizip-ng_BUILD_VERSION})
set (minizip-ng_REFIND TRUE)
set (minizip-ng_REFIND_VERSION ${minizip-ng_BUILD_VERSION})
set (minizip-ng_REFIND_ARGS REQUIRED)
