# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# nanobind by hand!
#
# Unlike most of our other bundled dependencies, nanobind isn't a library
# to compile and install -- it's a source tree plus a set of CMake helper
# functions (nanobind_add_module(), etc.) that get pulled into whichever
# project actually builds Python extension modules. Its own top-level
# CMakeLists.txt, configured standalone with NB_TEST=OFF, does nothing
# but copy its headers/sources/cmake helpers to an install prefix and
# generate a version file -- there's nothing to actually compile. That's
# the (unusually light) local build performed below.
######################################################################

# NOTE: We are sticking with autobuild of nanobind defaulting to 2.13.0,
# because nanobind 3.0+ requiers a minimum python of 3.10, whereas we
# still support python 3.9.
set_cache (nanobind_BUILD_VERSION 2.13.0 "nanobind version for local builds")
set (nanobind_GIT_REPOSITORY "https://github.com/wjakob/nanobind")
set_cache (nanobind_GIT_TAG "v${nanobind_BUILD_VERSION}"
           "nanobind git tag to checkout")
set_cache (nanobind_GIT_COMMIT "e2dc00f7a34f935c6cf91948776d59c4709e9fe6"
           "nanobind commit hash to verify tag against")

set (nanobind_LOCAL_SOURCE_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/nanobind")

# nanobind vendors tsl::robin_map as a git submodule (ext/robin_map), and
# its CMakeLists.txt hard-errors if that submodule isn't checked out.
# build_dependency_with_cmake()'s plain `git clone` doesn't init submodules,
# so fetch the source and its submodule ourselves before handing off to the
# shared helper below (which will find the directory already present and
# just checkout/verify the pinned tag against it).
if (NOT IS_DIRECTORY "${nanobind_LOCAL_SOURCE_DIR}")
    find_package (Git REQUIRED)
    message (STATUS "Cloning ${nanobind_GIT_REPOSITORY} @ ${nanobind_GIT_TAG}")
    execute_process (COMMAND ${GIT_EXECUTABLE} clone -q
                        -b ${nanobind_GIT_TAG} --depth 1
                        ${nanobind_GIT_REPOSITORY} ${nanobind_LOCAL_SOURCE_DIR})
    execute_process (COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1
                        -- ext/robin_map
                      WORKING_DIRECTORY ${nanobind_LOCAL_SOURCE_DIR})
endif ()

build_dependency_with_cmake(nanobind
    VERSION         ${nanobind_BUILD_VERSION}
    GIT_REPOSITORY  ${nanobind_GIT_REPOSITORY}
    GIT_TAG         ${nanobind_GIT_TAG}
    GIT_COMMIT      ${nanobind_GIT_COMMIT}
    CMAKE_ARGS
        # Skip nanobind's own test suite -- we only want its install
        # rules (headers, sources, and CMake helper functions), not a
        # build of its tests, which would otherwise need Python at
        # configure time and a lot of unnecessary compilation.
        -D NB_TEST=OFF
    )

# nanobind installs its CMake package config to <prefix>/nanobind/cmake, a
# layout that generic find_package() prefix search doesn't check, so point
# straight at it. (This is the same trick externalpackages.cmake's
# discover_nanobind_cmake_dir() uses for a pip install, via
# `python -m nanobind --cmake_dir`.)
set (nanobind_DIR "${nanobind_LOCAL_INSTALL_DIR}/nanobind/cmake" CACHE PATH
     "Path to the nanobind CMake package" FORCE)

# Signal to caller that we need to find again at the installed location
set (nanobind_REFIND TRUE)
set (nanobind_REFIND_ARGS CONFIG)
