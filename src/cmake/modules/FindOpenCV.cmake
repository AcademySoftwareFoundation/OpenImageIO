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

find_path (OpenCV_INCLUDE_DIR opencv/cv.h
           "${PROJECT_SOURCE_DIR}/include"
           "${OpenCV_DIR}/include"
           "$ENV{OpenCV_DIR}/include"
           /usr/local/include
           /opt/local/include
           /usr/local/opt/opencv3/include
           )
if (OpenCV_INCLUDE_DIR AND EXISTS "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp")
    file (STRINGS "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp" TMP REGEX "^#define CV_VERSION_EPOCH .*$")
    if (TMP)
        string (REGEX MATCHALL "[0-9]+" CV_VERSION_EPOCH ${TMP})
    endif ()
    file (STRINGS "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp" TMP REGEX "^#define CV_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_MAJOR ${TMP})
    file (STRINGS "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp" TMP REGEX "^#define CV_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_MINOR ${TMP})
    file (STRINGS "${OpenCV_INCLUDE_DIR}/opencv2/core/version.hpp" TMP REGEX "^#define CV_VERSION_REVISION .*$")
    string (REGEX MATCHALL "[0-9]+" CV_VERSION_REVISION ${TMP})
    if (CV_VERSION_EPOCH)
        set (OpenCV_VERSION "${CV_VERSION_EPOCH}.${CV_VERSION_MAJOR}.${CV_VERSION_MINOR}")
    else ()
        set (OpenCV_VERSION "${CV_VERSION_MAJOR}.${CV_VERSION_MINOR}.${CV_VERSION_REVISION}")
    endif ()
endif ()

set (libdirs "${PROJECT_SOURCE_DIR}/lib"
             "${OpenCV_DIR}/lib"
             "$ENV{OpenCV_DIR}/lib"
             /usr/local/lib
             /opt/local/lib
             /usr/local/opt/opencv3/lib
             )


set (opencv_components opencv_imgproc opencv_core)
if (NOT ${OpenCV_VERSION} VERSION_LESS 3.0.0)
    set (opencv_components opencv_videoio ${opencv_components})
else (NOT ${OpenCV_VERSION} VERSION_LESS 3.0.0)
    set (opencv_components opencv_videoio ${opencv_components})
endif ()
foreach (component ${opencv_components})
    find_library (${component}_lib
                  NAMES ${component}
                  PATHS ${libdirs} )
    if (${component}_lib)
        set (OpenCV_LIBS ${OpenCV_LIBS} ${${component}_lib})
    endif ()
endforeach ()

if (OpenCV_INCLUDE_DIR AND OpenCV_LIBS)
    set (OpenCV_FOUND TRUE)
endif ()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenCV
                                   REQUIRED_VARS OpenCV_LIBS OpenCV_INCLUDE_DIR
                                   VERSION_VAR OpenCV_VERSION )

MARK_AS_ADVANCED (OpenCV_VERSION
                  OpenCV_INCLUDE_DIR
                  OpenCV_LIBS
                  OpenCV_DEFINITIONS )
