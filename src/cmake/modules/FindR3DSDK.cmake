# - Try to find R3D SDK
#
#  Set RED_ROOT Environment variable to root of the SDK
#  Otherwise it will try to guess some plausible paths.
#
# Once done, this will define
#
#  RED_FOUND - system has R3D SDK
#  RED_INCLUDE_DIR - the include directory
#  RED_LIBRARIES - link these at build time.
#  RED_RUNTIME_LIBRARIES - Use this path (or copy the contents to a path)
#          for R3DSDK::InitializeSdk() at runtime.

include (FindPackageHandleStandardArgs)

# Assuming only C++11 and 64 bit support is needed.
# Set RED_LIB_NAME before invoking this file to override.
if (NOT DEFINED RED_LIB_NAME)
    set(RED_LIB_NAME R3DSDKPIC-cpp11)
endif()


if(DEFINED ENV{RED_ROOT})
    set(RED_ROOT $ENV{RED_ROOT})
    # message("Using R3D SDK Root: " ${RED_ROOT})
else()
    # message("Looking for R3D SDK")

    # The R3D SDK is supplied as a zip file that you can unpack anywhere.
    # So try some plausible locations and hope for the best.
    # Use a glob to try and avoid dealing with exact SDK version numbers.

    file(GLOB RED_SDK_PATHS "/opt/R3DSDKv*"
        "C:/R3DSDKv*"
        ${CMAKE_CURRENT_SOURCE_DIR}/../R3DSDKv*
        ${CMAKE_CURRENT_SOURCE_DIR}/../../R3DSDKv*
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../R3DSDKv*
        )
    message("  in " ${RED_SDK_PATHS})
    find_path(RED_ROOT Include/R3DSDK.h
              PATHS ${RED_SDK_PATHS}
             )
endif()

set(RED_INCLUDE_DIR ${RED_ROOT}/Include)
find_library(RED_LIBRARY NAMES ${RED_LIB_NAME}
             PATHS ${RED_ROOT}/Lib/linux64/
                   ${RED_ROOT}/Lib/win64/
             NO_DEFAULT_PATH
            )
                   
# handle the QUIETLY and REQUIRED arguments and set RED_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(RED DEFAULT_MSG RED_LIBRARY RED_INCLUDE_DIR)


# TODO : Ask somebody with a Mac to add support.
if(RED_FOUND)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(RED_LIBRARIES ${RED_LIBRARY} dl)
        set(RED_RUNTIME_LIBRARIES ${RED_ROOT}/Redistributable/linux)
    else()
        set(RED_LIBRARIES ${RED_LIBRARY})
        set(RED_RUNTIME_LIBRARIES ${RED_ROOT}/Redistributable/win)
    endif()
endif()
message("Runtime libs: " ${RED_RUNTIME_LIBRARIES})
mark_as_advanced(RED_LIBRARY RED_INCLUDE_DIR)
