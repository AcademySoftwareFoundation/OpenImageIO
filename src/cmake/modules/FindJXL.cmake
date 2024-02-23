# Module to find libjxl
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Will define:
# - JXL_FOUND
# - JXL_INCLUDES directory to include for libjxl headers
# - JXL_LIBRARIES libraries to link to

include (FindPackageHandleStandardArgs)

find_path(JXL_INCLUDE_DIR
  NAMES jxl/decode.h jxl/encode.h)
mark_as_advanced(JXL_INCLUDE_DIR)

find_library(JXL_LIBRARY
  NAMES jxl)
mark_as_advanced(JXL_LIBRARY)

find_library(JXL_THREADS_LIBRARY
  NAMES jxl_threads)
mark_as_advanced(JXL_THREADS_LIBRARY)

find_package_handle_standard_args(JXL
  REQUIRED_VARS JXL_LIBRARY JXL_THREADS_LIBRARY JXL_INCLUDE_DIR)

if(JXL_FOUND)
  set(JXL_LIBRARIES ${JXL_LIBRARY} ${JXL_THREADS_LIBRARY})
  set(JXL_INCLUDES ${JXL_INCLUDE_DIR})
endif(JXL_FOUND)
