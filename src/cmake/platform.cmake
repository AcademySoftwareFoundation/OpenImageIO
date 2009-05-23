###########################################################################
# Figure out what platform we're on, and set some variables appropriately

message (STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message (STATUS "CMAKE_SYSTEM_VERSION = ${CMAKE_SYSTEM_VERSION}")
message (STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")

if (UNIX)
    message (STATUS "Unix! ${CMAKE_SYSTEM_NAME}")
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set (platform "linux")
        set (CXXFLAGS "${CXXFLAGS} -DLINUX")
        if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
            set (platform "linux64")
            set (CXXFLAGS "${CXXFLAGS} -DLINUX64")
        endif ()
    endif ()
    if (APPLE)
        message (STATUS "Apple!")
        set (platform "macosx")
    endif ()
endif ()

if (WIN32)
    message (STATUS "Windows!")
    set (platform "windows")
endif ()

if (platform)
    message (STATUS "platform = ${platform}")
else ()
    message (FATAL_ERROR "'platform' not defined")
endif ()
