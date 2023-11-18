# Find libjxl
# Will define:
# - JXL_FOUND
# - JXL_INCLUDE_DIRS directory to include for libjxl headers
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
  set(JXL_INCLUDE_DIRS ${JXL_INCLUDE_DIR})
endif(JXL_FOUND)
