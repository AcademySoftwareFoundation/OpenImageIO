# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# Imath by hand!
######################################################################

set_cache (Imath_BUILD_VERSION 3.1.10 "Imath version for local builds")
set (Imath_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/Imath")
set (Imath_GIT_TAG "v${Imath_BUILD_VERSION}")
set_cache (Imath_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local Imath build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${Imath_BUILD_VERSION} Imath_VERSION_IDENT)

build_dependency_with_cmake(Imath
    VERSION         ${Imath_BUILD_VERSION}
    GIT_REPOSITORY  ${Imath_GIT_REPOSITORY}
    GIT_TAG         ${Imath_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${Imath_BUILD_SHARED_LIBS}
        # Don't built unnecessary parts of Imath
        -D BUILD_TESTING=OFF
        -D IMATH_BUILD_EXAMPLES=OFF
        -D IMATH_BUILD_PYTHON=OFF
        -D IMATH_BUILD_TESTING=OFF
        -D IMATH_BUILD_TOOLS=OFF
        -D IMATH_INSTALL_DOCS=OFF
        -D IMATH_INSTALL_PKG_CONFIG=OFF
        -D IMATH_INSTALL_TOOLS=OFF
        # Give the library a custom name and symbol namespace so it can't
        # conflict with any others in the system or linked into the same app.
        # not needed -D IMATH_NAMESPACE_CUSTOM=1
        # not needed -D IMATH_INTERNAL_NAMESPACE=${PROJ_NAMESPACE_V}_Imath_${Imath_VERSION_IDENT}
        -D IMATH_LIB_SUFFIX=_v${Imath_VERSION_IDENT}_${PROJ_NAMESPACE_V}
    )


# Signal to caller that we need to find again at the installed location
set (Imath_REFIND TRUE)
set (Imath_REFIND_ARGS CONFIG)
set (Imath_REFIND_VERSION ${Imath_BUILD_VERSION})

if (Imath_BUILD_SHARED_LIBS)
    install_local_dependency_libs (Imath Imath)
endif ()
