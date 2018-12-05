# - Find OpenCV
# Find the OpenCV library
# This module defines
#  OpenCV_VERSION, the version string of OpenCV
#  OpenCV_INCLUDE_DIR, where to find header files
#  OpenCV_LIBRARIES, the libraries needed to use OpenCV
#  OpenCV_DEFINITIONS, the definitions needed to use OpenCV

FIND_PACKAGE(PkgConfig)

IF(PKG_CONFIG_FOUND AND NOT LIBRAW_PATH)
   PKG_CHECK_MODULES(PC_LIBRAW QUIET libraw)
   SET(LibRaw_DEFINITIONS ${PC_LIBRAW_CFLAGS_OTHER})

   PKG_CHECK_MODULES(PC_LIBRAW_R QUIET libraw_r)
   SET(LibRaw_r_DEFINITIONS ${PC_LIBRAW_R_CFLAGS_OTHER})   
ENDIF()

find_path (OpenCV_INCLUDE_DIR
           NAMES opencv4/opencv2/opencv.hpp opencv2/opencv.hpp
           PATHS
           "${PROJECT_SOURCE_DIR}/include"
           "${OpenCV_DIR}/include"
           "${OpenCV_DIR}/include/opencv4"
           "$ENV{OpenCV_DIR}/include"
           /usr/local/include
           /opt/local/include
           /usr/local/opt/opencv4/include
           /usr/local/opt/opencv3/include
           PATH_SUFFIXES opencv4
           )

set (_ocv_include_root "${OpenCV_INCLUDE_DIR}")
if (OpenCV_INCLUDE_DIR AND EXISTS "${OpenCV_INCLUDE_DIR}/opencv4/opencv2/core/version.hpp")
    set (OpenCV_INCLUDE_DIR "${OpenCV_INCLUDE_DIR}/opencv4")
endif ()
set (_ocv_version_file "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp")
if (EXISTS "${_ocv_version_file}")
    file (STRINGS "${_ocv_version_file}" TMP REGEX "^#define CV_VERSION_EPOCH .*$")
    if (TMP)
        string (REGEX MATCHALL "[0-9]+" CV_VERSION_EPOCH ${TMP})
    endif ()
    file (STRINGS "${_ocv_version_file}" TMP REGEX "^#define CV_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_MAJOR ${TMP})
    file (STRINGS "${_ocv_version_file}" TMP REGEX "^#define CV_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_MINOR ${TMP})
    file (STRINGS "${_ocv_version_file}" TMP REGEX "^#define CV_VERSION_REVISION .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_REVISION ${TMP})
    if (CV_VERSION_EPOCH)
        set (OpenCV_VERSION "${CV_VERSION_EPOCH}.${CV_VERSION_MAJOR}.${CV_VERSION_MINOR}")
    else ()
        set (OpenCV_VERSION "${CV_VERSION_MAJOR}.${CV_VERSION_MINOR}.${CV_VERSION_REVISION}")
    endif ()
    message (STATUS "Found OpenCV ${OpenCV_VERSION} include in ${OpenCV_INCLUDE_DIR}")
endif ()

set (libdirs "${PROJECT_SOURCE_DIR}/lib"
             "${OpenCV_DIR}/lib"
             "$ENV{OpenCV_DIR}/lib"
             "${_ocv_include_root}/../lib"
             /usr/local/lib
             /opt/local/lib
             /usr/local/opt/opencv4/lib
             /usr/local/opt/opencv3/lib
             )

if (NOT ${OpenCV_VERSION} VERSION_LESS 4.0.0)
    set (opencv_components opencv_core opencv_imgproc opencv_videoio)
elseif (NOT ${OpenCV_VERSION} VERSION_LESS 3.0.0)
    set (opencv_components opencv_videoio opencv_imgproc opencv_core)
else (NOT ${OpenCV_VERSION} VERSION_LESS 2.0.0)
    set (opencv_components opencv_highgui opencv_imgproc opencv_core)
endif ()
foreach (component ${opencv_components})
    find_library (${component}_lib
                  NAMES ${component}
                  PATHS ${libdirs}
                  NO_DEFAULT_PATH)
    # If that didn't work, try again with default paths
    find_library (${component}_lib
                  NAMES ${component}
                  PATHS ${libdirs})
    if (${component}_lib)
        set (OpenCV_LIBS ${OpenCV_LIBS} ${${component}_lib})
    endif ()
endforeach ()

if (OpenCV_INCLUDE_DIR AND OpenCV_LIBS)
    set (OpenCV_FOUND TRUE)
    message (STATUS "Found OpenCV libs: ${OpenCV_LIBS}")
endif ()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenCV
                REQUIRED_VARS OpenCV_LIBS OpenCV_INCLUDE_DIR OpenCV_VERSION
                VERSION_VAR   OpenCV_VERSION )

MARK_AS_ADVANCED (OpenCV_VERSION
                  OpenCV_INCLUDE_DIR
                  OpenCV_LIBS
                  OpenCV_DEFINITIONS )
unset (_ocv_version_file)
unset (_ocv_include_root)
