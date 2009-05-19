###########################################################################
# Figure out what platform we're on, and set some variables appropriately

MESSAGE (STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
MESSAGE (STATUS "CMAKE_SYSTEM_VERSION = ${CMAKE_SYSTEM_VERSION}")
MESSAGE (STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")

IF ( UNIX )
    MESSAGE (STATUS "Unix! ${CMAKE_SYSTEM_NAME}")
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
        SET ( platform "linux" )
        SET ( CXXFLAGS "${CXXFLAGS} -DLINUX" )
        IF ( ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64" )
            SET ( platform "linux64" )
            SET ( CXXFLAGS "${CXXFLAGS} -DLINUX64" )
        ENDIF ()
    ENDIF ()
    IF ( APPLE )
        MESSAGE (STATUS "Apple!")
        SET ( platform "macosx" )
    ENDIF ()
ENDIF ()

IF ( WINDOWS )
    MESSAGE (STATUS "Windows!")
    SET ( platform "windows" )
ENDIF ()

IF ( platform )
    MESSAGE (STATUS "platform = ${platform}")
ELSE ()
    MESSAGE (FATAL_ERROR "'platform' not defined")
ENDIF ()
