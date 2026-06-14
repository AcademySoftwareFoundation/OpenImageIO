# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# pystring by hand!
######################################################################

set_cache (pystring_BUILD_VERSION 1.2.0 "pystring version for local builds")
set (pystring_GIT_REPOSITORY "https://github.com/imageworks/pystring")
set (pystring_GIT_TAG "v${pystring_BUILD_VERSION}" "Git branch or tag")
set (pystring_GIT_COMMIT "1922c8f2b48e3beb6831c27b4811b58995e986cf")

set_cache (pystring_BUILD_SHARED_LIBS OFF
           DOC "Should a local pystring build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${pystring_BUILD_VERSION} pystring_VERSION_IDENT)

build_dependency_with_cmake(pystring
    VERSION         ${pystring_BUILD_VERSION}
    GIT_REPOSITORY  ${pystring_GIT_REPOSITORY}
    GIT_TAG         ${pystring_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${pystring_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
)

set (pystring_VERSION ${pystring_BUILD_VERSION})
unset (PYSTRING_LIBRARY)
unset (PYSTRING_INCLUDE_DIR)

set (pystring_REFIND FALSE)
set (pystring_REFIND_VERSION ${pystring_BUILD_VERSION})


if (pystring_BUILD_SHARED_LIBS)
    install_local_dependency_libs (pystring pystring)
endif ()
