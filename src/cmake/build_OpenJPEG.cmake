# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/Academ SoftwareFoundation/OpenImageIO

set_cache (OpenJPEG_BUILD_VERSION 2.5.4 "OpenJPEG version for local builds")
set (OpenJPEG_GIT_REPOSITORY "https://github.com/uclouvain/openjpeg.git")
set (OpenJPEG_GIT_TAG "v${OpenJPEG_BUILD_VERSION}")
set_cache (OpenJPEG_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local OpenJPEG build, if necessary, build shared libraries" ADVANCED)


string (MAKE_C_IDENTIFIER ${OpenJPEG_BUILD_VERSION} OpenJPEG_VERSION_IDENT)

build_dependency_with_cmake(OpenJPEG
    VERSION         ${OpenJPEG_BUILD_VERSION}
    GIT_REPOSITORY  ${OpenJPEG_GIT_REPOSITORY}
    GIT_TAG         ${OpenJPEG_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_CODEC=OFF 
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
    )
# Set some things up that we'll need for a subsequent find_package to work
set (OpenJPEG_ROOT ${OpenJPEG_LOCAL_INSTALL_DIR})


# Signal to caller that we need to find again at the installed location
set (OpenJPEG_REFIND TRUE)
set (OpenJPEG_REFIND_ARGS CONFIG)
set_invert (OpenJPEG_LINKSTATIC ${OpenJPEG_BUILD_SHARED_LIBS})

if (OpenJPEG_BUILD_SHARED_LIBS)
    install_local_dependency_libs (OpenJPEG openjp2)
endif ()
