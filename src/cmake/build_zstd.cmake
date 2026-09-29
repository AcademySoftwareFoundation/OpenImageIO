# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# zstd by hand!
######################################################################

set_cache (zstd_BUILD_VERSION 1.5.7 "zstd version for local builds")
set (zstd_GIT_REPOSITORY "https://github.com/facebook/zstd")
set (zstd_GIT_TAG "v${zstd_BUILD_VERSION}")
set (zstd_GIT_COMMIT "f8745da6ff1ad1e7bab384bd1f9d742439278e99")
set_cache (zstd_BUILD_SHARED_LIBS OFF # ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local zstd build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${zstd_BUILD_VERSION} zstd_VERSION_IDENT)

if (zstd_BUILD_SHARED_LIBS)
    set (_zstd_build_static OFF)
else ()
    set (_zstd_build_static ON)
endif ()

build_dependency_with_cmake(zstd
    VERSION         ${zstd_BUILD_VERSION}
    GIT_REPOSITORY  ${zstd_GIT_REPOSITORY}
    GIT_TAG         ${zstd_GIT_TAG}
    GIT_COMMIT      ${zstd_GIT_COMMIT}
    SOURCE_SUBDIR   build/cmake
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${zstd_BUILD_SHARED_LIBS}
        -D ZSTD_BUILD_SHARED=${zstd_BUILD_SHARED_LIBS}
        -D ZSTD_BUILD_STATIC=${_zstd_build_static}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        -D ZSTD_BUILD_PROGRAMS=OFF
        -D ZSTD_BUILD_TESTS=OFF
        -D ZSTD_BUILD_CONTRIB=OFF
        -D ZSTD_LEGACY_SUPPORT=OFF
        -D ZSTD_MULTITHREAD_SUPPORT=ON
    )

# Signal to caller that we need to find again at the installed location
set (zstd_REFIND TRUE)
set (zstd_REFIND_ARGS CONFIG)
set (zstd_REFIND_VERSION ${zstd_BUILD_VERSION})

if (zstd_BUILD_SHARED_LIBS)
    install_local_dependency_libs (zstd zstd)
endif ()

unset (_zstd_build_static)
