# - Try to find libjpeg-turbo
# Once done, this will define
#
#  JPEG_FOUND - system has libjpeg-turbo
#  JPEG_INCLUDE_DIRS - the libjpeg-turbo include directories
#  JPEG_LIBRARIES - link these to use libjpeg-turbo
#

include (FindPackageHandleStandardArgs)

find_path(JPEG_INCLUDE_DIR turbojpeg.h
          PATHS ${JPEGTURBO_PATH}/include
                /usr/local/opt/jpeg-turbo/include)
set(JPEG_NAMES ${JPEG_NAMES} jpeg libjpeg turbojpeg libturbojpeg)

find_library(JPEG_LIBRARY NAMES ${JPEG_NAMES} 
             PATHS ${JPEG_INCLUDE_DIR}/../lib
                   ${JPEGTURBO_PATH}/lib64
                   ${JPEGTURBO_PATH}/lib
                   /usr/local/opt/jpeg-turbo/lib
                   /opt/libjpeg-turbo/lib64
                   /opt/libjpeg-turbo/lib
                   NO_DEFAULT_PATH)
if (NOT JPEG_LIBRARY)
find_library(JPEG_LIBRARY NAMES ${JPEG_NAMES} 
             PATHS ${JPEG_INCLUDE_DIR}/../lib
                   ${JPEGTURBO_PATH}/lib
                   /usr/local/opt/jpeg-turbo/lib)
endif ()

# handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JPEG DEFAULT_MSG JPEG_LIBRARY JPEG_INCLUDE_DIR)

if(JPEG_FOUND)
  set(JPEG_LIBRARIES ${JPEG_LIBRARY})
endif()

mark_as_advanced(JPEG_LIBRARY JPEG_INCLUDE_DIR )

