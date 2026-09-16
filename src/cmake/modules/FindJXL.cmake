# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# Module to find libjxl
#
# Will define:
# - JXL_FOUND
# - JXL_INCLUDES directory to include for libjxl headers
# - JXL_LIBRARIES libraries to link to

include (FindPackageHandleStandardArgs)

find_path(JXL_INCLUDE_DIR
  NAMES jxl/decode.h jxl/encode.h)
mark_as_advanced(JXL_INCLUDE_DIR)

if (JXL_INCLUDE_DIR)
    file (STRINGS "${JXL_INCLUDE_DIR}/jxl/version.h" TMP REGEX "^#define JPEGXL_MAJOR_VERSION .*$")
    string (REGEX MATCHALL "[0-9]+" JPEGXL_MAJOR_VERSION ${TMP})
    file (STRINGS "${JXL_INCLUDE_DIR}/jxl/version.h" TMP REGEX "^#define JPEGXL_MINOR_VERSION .*$")
    string (REGEX MATCHALL "[0-9]+" JPEGXL_MINOR_VERSION ${TMP})
    file (STRINGS "${JXL_INCLUDE_DIR}/jxl/version.h" TMP REGEX "^#define JPEGXL_PATCH_VERSION .*$")
    string (REGEX MATCHALL "[0-9]+" JPEGXL_PATCH_VERSION ${TMP})
    set (JXL_VERSION "${JPEGXL_MAJOR_VERSION}.${JPEGXL_MINOR_VERSION}.${JPEGXL_PATCH_VERSION}")
endif ()

find_library(JXL_LIBRARY
  NAMES jxl)
mark_as_advanced (
    JXL_LIBRARY
    JXL_VERSION
    )

find_library(JXL_THREADS_LIBRARY
  NAMES jxl_threads)
mark_as_advanced(JXL_THREADS_LIBRARY)

find_package_handle_standard_args(JXL
  REQUIRED_VARS JXL_LIBRARY JXL_THREADS_LIBRARY JXL_INCLUDE_DIR)

if(JXL_FOUND)
  set(JXL_LIBRARIES ${JXL_LIBRARY} ${JXL_THREADS_LIBRARY})
  set(JXL_INCLUDES ${JXL_INCLUDE_DIR})
  # A static libjxl does not carry its dependencies, which are installed
  # alongside it, so link those too.
  # ponytail: can't tell a static .lib from an import .lib on Windows, where
  # local builds default to shared anyway.
  if (JXL_LIBRARY MATCHES "\\.a$")
    get_filename_component (_jxl_libdir ${JXL_LIBRARY} DIRECTORY)
    foreach (_jxl_dep jxl_cms brotlienc brotlidec brotlicommon hwy)
      if (EXISTS ${_jxl_libdir}/lib${_jxl_dep}.a)
        list (APPEND JXL_LIBRARIES ${_jxl_libdir}/lib${_jxl_dep}.a)
      endif ()
    endforeach ()
  endif ()
endif(JXL_FOUND)
