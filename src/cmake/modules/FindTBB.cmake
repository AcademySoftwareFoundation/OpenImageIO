# Find the tbb library.
#
# Sets the usual variables expected for find_package scripts:
#
# TBB_INCLUDE_DIR - header location
# TBB_LIBRARIES - library to link against
# TBB_FOUND - true if pugixml was found.

find_path (TBB_INCLUDE_DIR tbb/tbb.h)
find_library (TBB_LIBRARY NAMES tbb)

# Support the REQUIRED and QUIET arguments, and set TBB_FOUND if found.
include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (TBB DEFAULT_MSG TBB_LIBRARY
                                   TBB_INCLUDE_DIR)

if (TBB_FOUND)
    set (TBB_LIBRARIES ${TBB_LIBRARY})
endif()

mark_as_advanced (TBB_LIBRARY TBB_INCLUDE_DIR)
