# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# ZLIB / ZLIB-NG by hand!
######################################################################

# This script builds either zlib or zlib-ng (in compatibility mode) from source.

# Zlib-ng is a fork of zlib that is actively maintained and has some
# performance improvments over the original zlib which pertain to 
# PNG and WebP plugin performance (on Windows, in particular).

# By default, we build zlib-ng in zlib API compatibility mode. This 
# allows OIIO and its dependencies to use zlib-ng v2.2.4 as a drop-in 
# replacement for zlib v1.3.1. 

# Please note -- this means that `find_package(ZLIB)` will recognize zlib-ng
# as zlib, and set ZLIB_VERSION = "1.3.1" (instead of zlib-ng's version)


# There are two ways to build against zlib instead of zlib-ng:

#   1. At build time, set `ENABLE_ZLIBNG=OFF` to
#      to build with the original zlib (defaults to ON).

#   2. When using the `checked_find_package` macro, require a version
#      less than 2.0.0, e.g. `find_package(ZLIB 1.3.1 REQUIRED)`.


set (ZLIB_VERSION_LATEST    "1.3.1")
set (ZLIBNG_VERSION_LATEST  "2.2.4")

# By default, we build zlib-ng in compatibility mode. 
# Set ENABLE_ZLIBNG=OFF to build the original zlib.
check_is_enabled(ZLIBNG ENABLE_ZLIBNG)


# Set the version to build; note that 1.x versions will build the original zlib,
# while 2.x or later will build zlib-ng.
if (ENABLE_ZLIBNG)
    set_cache (ZLIB_BUILD_VERSION ${ZLIBNG_VERSION_LATEST} "ZLIB or ZLIB-NG version for local builds")
else ()
    set_cache (ZLIB_BUILD_VERSION ${ZLIB_VERSION_LATEST} "ZLIB or ZLIB-NG version for local builds")
endif ()


# Choose the git repository, tag format, and extra arguments based on version.
# For now, we're assuming that the original zlib will never reach version 2.0, 
# so anything greater than or equal to 2.0 is zlib-ng.
if (ZLIB_BUILD_VERSION VERSION_LESS "2.0.0")
  # Original zlib: use the 'v' prefix in the tag.
  set (ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib")
  set (ZLIB_GIT_TAG "v${ZLIB_BUILD_VERSION}")
  set (ZLIB_BUILD_OPTIONS "")
  set (ZLIBNG_USED FALSE)
  if (ENABLE_ZLIBNG)
    message (STATUS "Building zlib version ${ZLIB_BUILD_VERSION}, even though ENABLE_ZLIBNG=${ENABLE_ZLIBNG}")
    message (STATUS "If you believe this is a mistake, check usages of `checked_find_package(ZLIB ...)` in the code base.")
    message (STATUS "See src/cmake/build_ZLIB.cmake for more details.")
  else ()
    message (STATUS "Building zlib version ${ZLIB_BUILD_VERSION}")
  endif ()
else ()
  # zlib-ng: omit the 'v' prefix for the git tag and enable compatibility mode.
  set (ZLIB_GIT_REPOSITORY "https://github.com/zlib-ng/zlib-ng")
  set (ZLIB_GIT_TAG "${ZLIB_BUILD_VERSION}")
  set (ZLIB_BUILD_OPTIONS "-DZLIB_COMPAT=ON;-DWITH_GTEST=OFF;-DWITH_GZFILEOP=OFF;-DZLIBNG_ENABLE_TESTS=OFF;-DZLIB_ENABLE_TESTS=OFF")
  set (ZLIBNG_USED TRUE)
  message (STATUS "Building zlib-ng version ${ZLIB_BUILD_VERSION}")
endif ()


set_cache (ZLIB_BUILD_SHARED_LIBS OFF
  DOC "Should execute a local ZLIB build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${ZLIB_BUILD_VERSION} ZLIB_VERSION_IDENT)

build_dependency_with_cmake (ZLIB
    VERSION         ${ZLIB_BUILD_VERSION}
    GIT_REPOSITORY  ${ZLIB_GIT_REPOSITORY}
    GIT_TAG         ${ZLIB_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${ZLIB_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        ${ZLIB_BUILD_OPTIONS}
    )

# Set some things up that we'll need for a subsequent find_package to work
set (ZLIB_ROOT ${ZLIB_LOCAL_INSTALL_DIR})


if (ZLIBNG_USED)
    # zlib-ng provides a CMake config file, so we can use find_package directly.
    find_package (ZLIB CONFIG REQUIRED HINTS ${ZLIB_LOCAL_INSTALL_DIR} NO_DEFAULT_PATH)

    # First, locate the directory containing zlib.h.
    find_path (ZLIB_INCLUDE_DIR
        NAMES zlib.h
        HINTS ${ZLIB_ROOT}/include
    )

    if (NOT ZLIB_INCLUDE_DIR)
        message (FATAL_ERROR "Could not locate zlib-ng include directory.")
    endif ()

    message (STATUS "Found zlib-ng header directory: ${ZLIB_INCLUDE_DIR}")

    # Read the contents of zlib.h
    file (READ "${ZLIB_INCLUDE_DIR}/zlib.h" ZLIB_HEADER_CONTENT)

    # Use a regular expression to search for the ZLIB_VERSION macro.
    # This regex looks for a line like: #define ZLIB_VERSION "1.3.1.zlib-ng"
    string (REGEX MATCH "#[ \t]*define[ \t]+ZLIB_VERSION[ \t]+\"([^\"]+)\"" _match "${ZLIB_HEADER_CONTENT}")

    if (_match)
    # The first capture group is stored in CMAKE_MATCH_1.
        set (ZLIB_VERSION "${CMAKE_MATCH_1}")
    endif ()
    

else ()
    # Vanilla ZLIB doesn't ship with a CMake config file, so we'll just "refind" it with the
    # usual arguments.
    set (ZLIB_REFIND TRUE)
    set (ZLIB_REFIND_VERSION ${ZLIB_BUILD_VERSION})
endif ()

if (ZLIB_BUILD_SHARED_LIBS)
    install_local_dependency_libs(ZLIB ZLIB)
endif()
