
find_library(ARNOLD_LIBRARY
  NAMES
    ai
  PATHS
    $ENV{ARNOLD_HOME}/bin
  DOC "Arnold library"
)

find_path(ARNOLD_INCLUDE_DIR ai.h
  PATHS
    $ENV{ARNOLD_HOME}/include
  DOC "Maya's library path"
)
 
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Arnold DEFAULT_MSG
  ARNOLD_LIBRARY ARNOLD_INCLUDE_DIR
)