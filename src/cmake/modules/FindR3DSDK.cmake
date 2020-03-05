# - Try to find R3D SDK
#
#  Set R3DSDK_ROOT Environment variable to root of the SDK
#  Otherwise it will try to guess some plausible paths.
#
# Once done, this will define
#
#  R3DSDK_FOUND - system has R3D SDK
#  R3DSDK_INCLUDE_DIR - the include directory
#  R3DSDK_LIBRARIES - link these at build time.
#  R3DSDK_RUNTIME_LIBRARIES - Use this path (or copy the contents to a path)
#          for R3DSDK::InitializeSdk() at runtime.

include (FindPackageHandleStandardArgs)

# Assuming only C++11 and 64 bit support is needed.
# Set R3DSDK_LIB_NAME before invoking this file to override.
if (NOT DEFINED R3DSDK_LIB_NAME)
    set(R3DSDK_LIB_NAME R3DSDKPIC-cpp11)
endif()


if(DEFINED ENV{R3DSDK_ROOT})
    set(R3DSDK_ROOT $ENV{R3DSDK_ROOT})
    # message("Using R3D SDK Root: " ${R3DSDK_ROOT})
else()
    # message("Looking for R3D SDK")

    # The R3D SDK is supplied as a zip file that you can unpack anywhere.
    # So try some plausible locations and hope for the best.
    # Use a glob to try and avoid dealing with exact SDK version numbers.

    file(GLOB R3DSDK_PATHS "/opt/R3DSDKv*"
        "C:/R3DSDKv*"
        ${CMAKE_CURRENT_SOURCE_DIR}/../R3DSDKv*
        ${CMAKE_CURRENT_SOURCE_DIR}/../../R3DSDKv*
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../R3DSDKv*
        )
    find_path(R3DSDK_ROOT Include/R3DSDK.h
              PATHS ${R3DSDK_PATHS}
             )
endif()

set(R3DSDK_INCLUDE_DIR ${R3DSDK_ROOT}/Include)
find_library(R3DSDK_LIBRARY NAMES ${R3DSDK_LIB_NAME}
             PATHS ${R3DSDK_ROOT}/Lib/linux64/
                   ${R3DSDK_ROOT}/Lib/win64/
             NO_DEFAULT_PATH
            )

# handle the QUIETLY and REQUIRED arguments and set R3DSDK_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(R3DSDK DEFAULT_MSG R3DSDK_LIBRARY R3DSDK_INCLUDE_DIR)


# TODO : Ask somebody with a Mac to add support.
if(R3DSDK_FOUND)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(R3DSDK_LIBRARIES ${R3DSDK_LIBRARY} dl)
        set(R3DSDK_RUNTIME_LIBRARIES ${R3DSDK_ROOT}/Redistributable/linux)
    else()
        set(R3DSDK_LIBRARIES ${R3DSDK_LIBRARY})
        set(R3DSDK_RUNTIME_LIBRARIES ${R3DSDK_ROOT}/Redistributable/win)
    endif()
endif()
mark_as_advanced(R3DSDK_LIBRARY R3DSDK_INCLUDE_DIR)
