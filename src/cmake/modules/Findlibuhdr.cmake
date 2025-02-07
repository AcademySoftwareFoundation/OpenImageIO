# Module to find libuhdr
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# This module defines the following variables:
#
# libuhdr_FOUND            True if libuhdr was found.
# LIBUHDR_INCLUDE_DIR      Where to find libuhdr headers
# LIBUHDR_LIBRARY          Library for uhdr

include (FindPackageHandleStandardArgs)

find_path(LIBUHDR_INCLUDE_DIR
  NAMES
    ultrahdr_api.h
  PATH_SUFFIXES
    include
)

find_library(LIBUHDR_LIBRARY uhdr
  PATH_SUFFIXES
    lib
)

find_package_handle_standard_args (libuhdr
    REQUIRED_VARS   LIBUHDR_INCLUDE_DIR
                    LIBUHDR_LIBRARY
    )
