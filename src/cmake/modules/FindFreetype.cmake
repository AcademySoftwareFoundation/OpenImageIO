# Module to find FreeType
#
# This module will first look into the directories defined by the variables:
#   - FREETYPE_PATH, FREETYPE_INCLUDE_PATH, FREETYPE_LIBRARY_PATH
#
# This module defines the following variables:
#
# FREETYPE_FOUND       - True if Freetype was found.
# FREETYPE_INCLUDE_DIRS -    where to find Freetype.h
# FREETYPE_LIBRARIES   - list of libraries to link against when using Freetype

# Other standarnd issue macros
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

FIND_PATH (FREETYPE_INCLUDE_DIRS ft2build.h
    ${FREETYPE_INCLUDE_PATH}
    ${FREETYPE_PATH}/include/
    /usr/include
    /usr/include/freetype2
    /usr/include/freetype2/freetype
    /usr/local/include
    /usr/local/include/freetype2
    /usr/local/include/freetype2/freetype
    /sw/include
    /opt/local/include
    DOC "The directory where freetype.h resides")
FIND_LIBRARY (FREETYPE_LIBRARIES
    NAMES FREETYPE freetype
    PATHS
    ${FREETYPE_LIBRARY_PATH}
    ${FREETYPE_PATH}/lib/
    /usr/lib64
    /usr/lib
    /usr/local/lib64
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    DOC "The FREETYPE library")
if (FREETYPE_INCLUDE_DIRS AND FREETYPE_LIBRARIES)
    set (FREETYPE_FOUND TRUE)
    list (APPEND FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS}/freetype2
                                       ${FREETYPE_INCLUDE_DIRS}/freetype2/freetype)
    if (VERBOSE)
        message (STATUS "Found FREETYPE library ${FREETYPE_LIBRARIES}")
        message (STATUS "Found FREETYPE includes ${FREETYPE_INCLUDE_DIRS}")
    endif ()
else()
    set (FREETYPE_FOUND FALSE)
    message (STATUS "FREETYPE not found. Specify FREETYPE_PATH to locate it")
endif()

