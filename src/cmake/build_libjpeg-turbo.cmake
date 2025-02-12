# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/Academ SoftwareFoundation/OpenImageIO

set_cache (libjpeg-turbo_BUILD_VERSION 3.0.4 "libjpeg-turbo version for local builds")
set (libjpeg-turbo_GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo")
set (libjpeg-turbo_GIT_TAG "${libjpeg-turbo_BUILD_VERSION}")
set_cache (libjpeg-turbo_BUILD_SHARED_LIBS OFF #${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local libjpeg-turbo build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${libjpeg-turbo_BUILD_VERSION} libjpeg-turbo_VERSION_IDENT)

build_dependency_with_cmake(libjpeg-turbo
    VERSION         ${libjpeg-turbo_BUILD_VERSION}
    GIT_REPOSITORY  ${libjpeg-turbo_GIT_REPOSITORY}
    GIT_TAG         ${libjpeg-turbo_GIT_TAG}
    CMAKE_ARGS
        -D ENABLE_SHARED=${libjpeg-turbo_BUILD_SHARED_LIBS}
        -D WITH_JPEG8=1
        -D CMAKE_INSTALL_LIBDIR=lib
        -D CMAKE_POSITION_INDEPENDENT_CODE=1

    )
# Set some things up that we'll need for a subsequent find_package to work
set (libjpeg-turbo_ROOT ${libjpeg-turbo_LOCAL_INSTALL_DIR})


# Signal to caller that we need to find again at the installed location
set (libjpeg-turbo_REFIND TRUE)
set (libjpeg-turbo_REFIND_ARGS CONFIG)

if (libjpeg-turbo_BUILD_SHARED_LIBS)
    install_local_dependency_libs (libjpeg-turbo libjpeg-turbo)
endif ()
