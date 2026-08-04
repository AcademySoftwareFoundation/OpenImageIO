# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# APV (OpenAPV) by hand!
######################################################################

set_cache (APV_BUILD_VERSION 0.3.0.0 "OpenAPV version for local builds")
set (APV_GIT_REPOSITORY "https://github.com/openapv/openapv")
set_cache (APV_GIT_TAG "v${APV_BUILD_VERSION}" "Git branch or tag")
set_cache (APV_GIT_COMMIT "9f6fd2a7369db90acec67d99fc57724f1136fb84"
           "commit hash to verify tag against")
set_cache (APV_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local APV build, if necessary, build shared libraries" ADVANCED)

if (APV_BUILD_SHARED_LIBS)
    set (_apv_lib_args -D OAPV_BUILD_SHARED_LIB=ON -D OAPV_BUILD_STATIC_LIB=OFF)
else ()
    set (_apv_lib_args -D OAPV_BUILD_SHARED_LIB=OFF -D OAPV_BUILD_STATIC_LIB=ON)
endif ()

build_dependency_with_cmake(APV
    VERSION         ${APV_BUILD_VERSION}
    GIT_REPOSITORY  ${APV_GIT_REPOSITORY}
    GIT_TAG         ${APV_GIT_TAG}
    GIT_COMMIT      ${APV_GIT_COMMIT}
    CMAKE_ARGS
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        -D OAPV_BUILD_APPS=OFF
        ${_apv_lib_args}
    )
unset (_apv_lib_args)

set (APV_ROOT ${APV_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location.
# N.B. the library reports its own apiset-based version from oapv.h, which
# does not match the release tag, so don't pass a REFIND_VERSION here.
set (APV_REFIND TRUE)

if (APV_BUILD_SHARED_LIBS)
    install_local_dependency_libs (APV oapv)
endif ()
