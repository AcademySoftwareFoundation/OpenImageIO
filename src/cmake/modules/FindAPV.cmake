# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# Module to find the OpenAPV library (APV codec)
# https://github.com/openapv/openapv
#
# Will define:
# - APV_FOUND
# - APV_INCLUDES directory to include for oapv headers
# - APV_LIBRARIES libraries to link to
# - APV_VERSION library version (apiset.major.minor.patch from oapv.h)

include (FindPackageHandleStandardArgs)

find_path(APV_INCLUDE_DIR
  NAMES oapv/oapv.h)
mark_as_advanced(APV_INCLUDE_DIR)

if (APV_INCLUDE_DIR)
    file (STRINGS "${APV_INCLUDE_DIR}/oapv/oapv.h" TMP REGEX "^#define OAPV_VER_APISET .*$")
    string (REGEX MATCHALL "[0-9]+" OAPV_VER_APISET ${TMP})
    file (STRINGS "${APV_INCLUDE_DIR}/oapv/oapv.h" TMP REGEX "^#define OAPV_VER_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" OAPV_VER_MAJOR ${TMP})
    file (STRINGS "${APV_INCLUDE_DIR}/oapv/oapv.h" TMP REGEX "^#define OAPV_VER_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" OAPV_VER_MINOR ${TMP})
    file (STRINGS "${APV_INCLUDE_DIR}/oapv/oapv.h" TMP REGEX "^#define OAPV_VER_PATCH .*$")
    string (REGEX MATCHALL "[0-9]+" OAPV_VER_PATCH ${TMP})
    set (APV_VERSION "${OAPV_VER_APISET}.${OAPV_VER_MAJOR}.${OAPV_VER_MINOR}.${OAPV_VER_PATCH}")
endif ()

# N.B. OpenAPV installs its libraries into a lib/oapv subdirectory.
find_library(APV_LIBRARY
  NAMES oapv liboapv
  PATH_SUFFIXES oapv)
mark_as_advanced (
    APV_LIBRARY
    APV_VERSION
    )

find_package_handle_standard_args(APV
  REQUIRED_VARS APV_LIBRARY APV_INCLUDE_DIR
  VERSION_VAR APV_VERSION)

if (APV_FOUND)
  set(APV_LIBRARIES ${APV_LIBRARY})
  set(APV_INCLUDES ${APV_INCLUDE_DIR})
  # A static liboapv has no generated oapv_exports.h; consumers must
  # define OAPV_STATIC_DEFINE so oapv.h doesn't try to include it.
  get_filename_component(_apv_lib_ext ${APV_LIBRARY} LAST_EXT)
  if (_apv_lib_ext STREQUAL ".a")
      set (APV_DEFINITIONS OAPV_STATIC_DEFINE)
  endif ()
  unset (_apv_lib_ext)
endif ()
