# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


set_cache (OpenEXR_BUILD_VERSION 3.2.4 "OpenEXR version for local builds")
set (OpenEXR_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/OpenEXR")
set (OpenEXR_GIT_TAG "v${OpenEXR_BUILD_VERSION}")
set_cache (OpenEXR_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local OpenEXR build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${OpenEXR_BUILD_VERSION} OpenEXR_VERSION_IDENT)

build_dependency_with_cmake(OpenEXR
    VERSION         ${OpenEXR_BUILD_VERSION}
    GIT_REPOSITORY  ${OpenEXR_GIT_REPOSITORY}
    GIT_TAG         ${OpenEXR_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${OpenEXR_BUILD_SHARED_LIBS}
        -D OPENEXR_FORCE_INTERNAL_DEFLATE=ON
        # Don't built unnecessary parts of OpenEXR
        -D BUILD_TESTING=OFF
        -D BUILD_WEBSITE=OFF
        -D OPENEXR_BUILD_EXAMPLES=OFF
        -D OPENEXR_BUILD_PYTHON=OFF
        -D OPENEXR_BUILD_SHARED_LIBS=OFF
        -D OPENEXR_BUILD_TOOLS=OFF
        -D OPENEXR_BUILD_WEBSITE=OFF
        -D OPENEXR_INSTALL_DOCS=OFF
        -D OPENEXR_INSTALL_PKG_CONFIG=OFF
        -D OPENEXR_INSTALL_TOOLS=OFF
        # Give the library a custom name and symbol namespace so it can't
        # conflict with any others in the system or linked into the same app.
        -D OPENEXR_NAMESPACE_CUSTOM=1
        -D ILMTHREAD_NAMESPACE_CUSTOM=1
        -D IEX_NAMESPACE_CUSTOM=1
        -D OPENEXR_INTERNAL_IMF_NAMESPACE=${PROJ_NAMESPACE_V}_Imf_${OpenEXR_VERSION_IDENT}
        -D ILMTHREAD_INTERNAL_NAMESPACE=${PROJ_NAMESPACE_V}_IlmThread_${OpenEXR_VERSION_IDENT}
        -D Iex_INTERNAL_NAMESPACE=${PROJ_NAMESPACE_V}_Iex_${OpenEXR_VERSION_IDENT}
        -D OPENEXR_LIB_SUFFIX=_v${OpenEXR_VERSION_IDENT}_${PROJ_NAMESPACE_V}
    )


# Signal to caller that we need to find again at the installed location
set (OpenEXR_REFIND TRUE)
set (OpenEXR_REFIND_ARGS CONFIG)
set (OpenEXR_REFIND_VERSION ${OpenEXR_BUILD_VERSION})

if (OpenEXR_BUILD_SHARED_LIBS)
    install_local_dependency_libs (OpenEXR OpenEXR)
    install_local_dependency_libs (OpenEXR IlmThread)
    install_local_dependency_libs (OpenEXR Iex)
endif ()
