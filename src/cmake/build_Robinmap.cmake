# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# Robinmap by hand!
######################################################################

set_cache (Robinmap_BUILD_VERSION 1.4.1 "Robinmap version for local builds")
set (Robinmap_GIT_REPOSITORY "https://github.com/Tessil/robin-map")
set (Robinmap_GIT_TAG "v${Robinmap_BUILD_VERSION}")
set_cache (Robinmap_GIT_COMMIT "bd14e6830a1474fed9d2d03f5c3b0683d818d540"
           "commit hash to verify tag against")

build_dependency_with_cmake(Robinmap
    VERSION         ${Robinmap_BUILD_VERSION}
    GIT_REPOSITORY  ${Robinmap_GIT_REPOSITORY}
    GIT_TAG         ${Robinmap_GIT_TAG}
    GIT_COMMIT      ${Robinmap_GIT_COMMIT}
    CMAKE_ARGS
        # Fix for pybind11 breaking against cmake 4.0.
        # Remove when pybind11 is fixed to declare its own minimum high enough.
        -D CMAKE_POLICY_VERSION_MINIMUM=3.5
    )

# Signal to caller that we need to find again at the installed location,
# this time via the real upstream package name (see the NAMES tsl-robin-map
# on the checked_find_package call in externalpackages.cmake) so that we
# get the genuine tsl::robin_map target -- the same one nanobind looks for.
set (Robinmap_REFIND TRUE)
set (Robinmap_REFIND_ARGS CONFIG NAMES tsl-robin-map)
set (Robinmap_REFIND_VERSION ${Robinmap_BUILD_VERSION})
