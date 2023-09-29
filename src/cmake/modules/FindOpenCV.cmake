# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# - Find OpenCV
# Find the OpenCV library
# This module defines
#  OpenCV_VERSION, the version string of OpenCV
#  OpenCV_INCLUDES, where to find header files
#  OpenCV_LIBRARIES, the libraries needed to use OpenCV
#  OpenCV_DEFINITIONS, the definitions needed to use OpenCV
#
# You can provide a location hint with OpenCV_ROOT (either a defined CMake
# variable or an environment variable).

find_path (OpenCV_INCLUDE_DIR
           NAMES opencv4/opencv2/opencv.hpp opencv2/opencv.hpp
           PATHS
               /opt/local/include
               /usr/local/opt/opencv4
               /usr/local/opt/opencv3
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
        string(REPLACE "." "" OpenCV_VERSION_SUFFIX "${OpenCV_VERSION}")
    else ()
        set (OpenCV_VERSION "${CV_VERSION_MAJOR}.${CV_VERSION_MINOR}.${CV_VERSION_REVISION}")
        string(REPLACE "." "" OpenCV_VERSION_SUFFIX "${OpenCV_VERSION}")
    endif ()
endif ()

set (libdirs "${PROJECT_SOURCE_DIR}/lib"
             "${_ocv_include_root}/../lib"
             /usr/local/lib
             /opt/local/lib
             /usr/local/opt/opencv4/lib
             /usr/local/opt/opencv3/lib
             )

set (opencv_components opencv_core opencv_imgproc opencv_videoio opencv_core${OpenCV_VERSION_SUFFIX} opencv_imgproc${OpenCV_VERSION_SUFFIX} opencv_videoio${OpenCV_VERSION_SUFFIX})
foreach (component ${opencv_components})
    find_library (${component}_lib
                  NAMES ${component}
                  HINTS ${libdirs})
    if (${component}_lib)
        set (OpenCV_LIBS ${OpenCV_LIBS} ${${component}_lib})
    endif ()
endforeach ()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenCV
                REQUIRED_VARS OpenCV_LIBS OpenCV_INCLUDE_DIR OpenCV_VERSION
                VERSION_VAR   OpenCV_VERSION )

if (OPENCV_FOUND)
    set (OpenCV_INCLUDES ${OpenCV_INCLUDE_DIR})
    set (OpenCV_LIBRARIES ${OpenCV_LIBS})
    foreach (component ${opencv_components})
        list (APPEND OpenCV_${component}_LIBRARIES ${${component}_lib})
    endforeach ()
endif ()

MARK_AS_ADVANCED (OpenCV_INCLUDE_DIR OpenCV_LIBS)
unset (_ocv_version_file)
unset (_ocv_include_root)
