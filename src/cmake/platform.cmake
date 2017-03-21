###########################################################################
# Figure out what platform we're on, and set some variables appropriately

# CMAKE_SYSTEM_PROCESSOR should not be used because it indicates the platform
# we are building on, but when cross compiling or using a chroot this is not
# what we want to use
if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set (SYSTEM_PROCESSOR "x86_64")
else()
    set (SYSTEM_PROCESSOR "i686")
endif()

if (VERBOSE)
    message (STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
    message (STATUS "CMAKE_SYSTEM_VERSION = ${CMAKE_SYSTEM_VERSION}")
    message (STATUS "SYSTEM_PROCESSOR = ${SYSTEM_PROCESSOR}")
endif ()

if (UNIX)
    if (VERBOSE)
        message (STATUS "Unix! ${CMAKE_SYSTEM_NAME}")
    endif ()
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set (platform "linux")
        set (CXXFLAGS "${CXXFLAGS} -DLINUX")
        if (${SYSTEM_PROCESSOR} STREQUAL "x86_64")
            set (platform "linux64")
            set (CXXFLAGS "${CXXFLAGS} -DLINUX64")
        endif ()
    elseif (APPLE)
        set (platform "macosx")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        set (platform "FreeBSD")
        set (CXXFLAGS "${CXXFLAGS} -DFREEBSD")
        if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
            # minimum arch of i586 is needed for atomic cpu instructions
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=i586")
        endif()
    else ()
        string (TOLOWER ${CMAKE_SYSTEM_NAME} platform)
    endif ()
endif ()

if (WIN32)
    set (platform "windows")
endif ()

if (platform)
    message (STATUS "platform = ${platform}")
else ()
    message (FATAL_ERROR "'platform' not defined")
endif ()

unset(SYSTEM_PROCESSOR)
