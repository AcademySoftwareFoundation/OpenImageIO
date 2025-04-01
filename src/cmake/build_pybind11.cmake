# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# pybind11 by hand!
######################################################################

set_cache (pybind11_BUILD_VERSION 2.12.0 "pybind11 version for local builds")
set (pybind11_GIT_REPOSITORY "https://github.com/pybind/pybind11")
set (pybind11_GIT_TAG "v${pybind11_BUILD_VERSION}")
set_cache (pybind11_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local pybind11 build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${pybind11_BUILD_VERSION} pybind11_VERSION_IDENT)

build_dependency_with_cmake(pybind11
    VERSION         ${pybind11_BUILD_VERSION}
    GIT_REPOSITORY  ${pybind11_GIT_REPOSITORY}
    GIT_TAG         ${pybind11_GIT_TAG}
    CMAKE_ARGS
        -D PYBIND11_PYTHON_VERSION=${PYTHON3_VERSION}
        # Don't built unnecessary parts of Pybind11
        -D BUILD_TESTING=OFF
        -D PYBIND11_TEST=OFF
        # Fix for pybind11 breaking against cmake 4.0.
        # Remove when pybind11 is fixed to declare its own minimum high enough.
        -D CMAKE_POLICY_VERSION_MINIMUM=3.5
    )


# Signal to caller that we need to find again at the installed location
set (pybind11_REFIND TRUE)
set (pybind11_REFIND_ARGS CONFIG)
set (pybind11_REFIND_VERSION ${pybind11_BUILD_VERSION})

if (pybind11_BUILD_SHARED_LIBS)
    install_local_dependency_libs (pybind11 pybind11)
endif ()
