cmake_minimum_required(VERSION 2.8)

project(OpenImageIO)

include(RezBuild)

rez_install_dirs(
    include lib python share
    DESTINATION .
)

# Bin specified separately so it can be marked as EXECUTABLE, otherwise it
# installs with only read permissions.
rez_install_dirs(
    bin
    DESTINATION .
    EXECUTABLE
)

rez_install_cmake(
    DESTINATION .
    INCLUDE_DIRS include
    LIBRARY_DIRS lib
    LIBRARIES OpenImageIO OpenImageIO_util
              OpenImageIO_d OpenImageIO_util_d
)
