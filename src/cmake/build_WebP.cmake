# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# WebP by hand!
######################################################################

set_cache (WebP_BUILD_VERSION 1.4.0 "WebP version for local builds")
set (WebP_GIT_REPOSITORY "https://github.com/webmproject/libwebp.git")
set (WebP_GIT_TAG "v${WebP_BUILD_VERSION}")

set_cache (WebP_BUILD_SHARED_LIBS OFF 
           DOC "Should execute a local WebP build; if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${WebP_BUILD_VERSION} WebP_VERSION_IDENT)

build_dependency_with_cmake(WebP
    VERSION         ${WebP_BUILD_VERSION}
    GIT_REPOSITORY  ${WebP_GIT_REPOSITORY}
    GIT_TAG         ${WebP_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${WebP_BUILD_SHARED_LIBS}
        -D WEBP_BUILD_ANIM_UTILS=OFF
        -D WEBP_BUILD_CWEBP=OFF
        -D WEBP_BUILD_DWEBP=OFF
        -D WEBP_BUILD_GIF2WEBP=OFF
        -D WEBP_BUILD_IMG2WEBP=OFF
        -D WEBP_BUILD_VWEBP=OFF
        -D WEBP_BUILD_WEBPINFO=OFF
        -D WEBP_BUILD_WEBPMUX=OFF
        -D WEBP_BUILD_EXTRAS=OFF
        -D CMAKE_INSTALL_LIBDIR=lib
    )

# Set some things up that we'll need for a subsequent find_package to work
set (WebP_ROOT ${WebP_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (WebP_REFIND TRUE)
set (WebP_REFIND_VERSION ${WebP_BUILD_VERSION})
set (WebP_REFIND_ARGS CONFIG)

if (WebP_BUILD_SHARED_LIBS)
    install_local_dependency_libs (WebP WebP)
endif ()
