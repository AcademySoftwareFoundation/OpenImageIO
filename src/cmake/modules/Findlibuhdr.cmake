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
