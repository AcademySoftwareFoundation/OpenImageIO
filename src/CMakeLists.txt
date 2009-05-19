PROJECT (OpenImageIO)
SET ( OpenImageIO_VERSION_MAJOR 0 )
SET ( OpenImageIO_VERSION_MINOR 5 )
SET ( OpenImageIO_VERSION_PATCH 0 )

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
SET (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)
MESSAGE ( STATUS "Project source dir = ${PROJECT_SOURCE_DIR}" )
MESSAGE ( STATUS "Project build dir = ${CMAKE_BINARY_DIR}" )

#PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
#    MESSAGE ( FATAL_ERROR "Not allowed to run in-source build!" )
#ENDIF ()


IF ( NOT CMAKE_BUILD_TYPE ) 
    SET( CMAKE_BUILD_TYPE "Release" ) 
ENDIF ()

SET ( EMBEDPLUGINS OFF CACHE BOOL "Embed format plugins in libOpenImageIO" )
SET ( USE_OPENGL ON CACHE BOOL "Include OpenGL support" )
SET ( USE_QT ON CACHE BOOL "Include Qt support" )

SET(CMAKE_MODULE_PATH
	"${PROJECT_SOURCE_DIR}/cmake/modules"
	"${PROJECT_SOURCE_DIR}/cmake")

INCLUDE ( util_macros )
INCLUDE ( platform )
INCLUDE ( externalpackages )


INCLUDE_DIRECTORIES ("include")

IF ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    ADD_DEFINITIONS ( "-DDEBUG=1" )
ENDIF ()

SET (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")



# Macro to add a build target for an IO plugin.
#
# Usage:
#
# ADD_OIIO_PLUGIN ( source1 [source2 ...]
#                   [LINK_LIBRARIES external_lib1 ...] )
#
# The plugin name is deduced from the name of the current directory and the
# source is automatically linked against OpenImageIO.  Additional libraries
# (for example, libpng) may be specified after the optionl LINK_LIBRARIES
# keyword.
#
MACRO ( ADD_OIIO_PLUGIN )
    PARSE_ARGUMENTS(_plugin "LINK_LIBRARIES" "" ${ARGN})
    SET ( _target_name ${CMAKE_CURRENT_SOURCE_DIR})
    # Get the name of the current directory and use it as the target name.
    GET_FILENAME_COMPONENT ( _target_name ${CMAKE_CURRENT_SOURCE_DIR} NAME )
    ADD_LIBRARY ( ${_target_name} SHARED ${_plugin_DEFAULT_ARGS} )
    TARGET_LINK_LIBRARIES ( ${_target_name} OpenImageIO ${_plugin_LINK_LIBRARIES} )
    SET_TARGET_PROPERTIES ( ${_target_name} PROPERTIES PREFIX "" )
    INSTALL ( TARGETS ${_target_name} LIBRARY DESTINATION lib )
ENDMACRO ()


###########################################################################


# Tell CMake to process the sub-directories
ADD_SUBDIRECTORY ( libOpenImageIO )

ADD_SUBDIRECTORY ( iconvert )
ADD_SUBDIRECTORY ( idiff )
ADD_SUBDIRECTORY ( igrep )
ADD_SUBDIRECTORY ( iinfo )
ADD_SUBDIRECTORY ( maketx )
ADD_SUBDIRECTORY ( iv )

# Add IO plugin directories
IF ( NOT EMBEDPLUGINS )
    ADD_SUBDIRECTORY ( bmp.imageio )
    ADD_SUBDIRECTORY ( hdr.imageio )
    ADD_SUBDIRECTORY ( ico.imageio )
    ADD_SUBDIRECTORY ( jpeg.imageio )
    ADD_SUBDIRECTORY ( openexr.imageio )
    ADD_SUBDIRECTORY ( png.imageio )
    ADD_SUBDIRECTORY ( tiff.imageio )
    ADD_SUBDIRECTORY ( zfile.imageio )
ENDIF ()

ADD_SUBDIRECTORY ( include )
ADD_SUBDIRECTORY ( doc )



# Testing
INCLUDE ( CTest )
ADD_SUBDIRECTORY ( testtex )
ADD_SUBDIRECTORY ( test_libOpenImageIO )
ADD_TEST ( libOpenImageIO ${CMAKE_BINARY_DIR}/test_libOpenImageIO/test_libOpenImageIO )
# Ugh, I can't seem to ADD_SUBDIRECTORY the testsuite dir if it's not a true
# subdirectory of source, so we fake it by creating a link.  FIXME!
EXEC_PROGRAM ( "cmake -E create_symlink ${PROJECT_SOURCE_DIR}/../testsuite ${PROJECT_SOURCE_DIR}/testsuite" )
ADD_SUBDIRECTORY ( testsuite )


# Packaging
SET ( CPACK_PACKAGE_VERSION_MAJOR ${OpenImageIO_VERSION_MAJOR} )
SET ( CPACK_PACKAGE_VERSION_MINOR ${OpenImageIO_VERSION_MINOR} )
SET ( CPACK_PACKAGE_VERSION_PATCH ${OpenImageIO_VERSION_PATCH} )
#SET ( CPACK_PACKAGE_DESCRIPTION_FILE ...FIXME... )
#SET ( CPACK_PACKAGE_DESCRIPTION_SUMMARY ...FIXME... )
#SET ( CPACK_PACKAGE_DESCRIPTION_FILE ...FIXME... )
SET ( CPACK_PACKAGE_FILE_NAME OpenImageIO-${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}-${platform} )
#SET ( CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_SOURCE_DIR}/.. )
#SET ( CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/../LICENSE" )
#SET ( CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/../README" )
#SET ( CPACK_RESOURCE_FILE_WELCOME ...FIXME... )
#SET ( CPACK_PACKAGE_EXECUTABLES iv )
#SET ( CPACK_STRIP_FILES ...FIXME... )
SET ( CPACK_SOURCE_PACKAGE_FILE_NAME OpenImageIO-${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}-source )
#SET ( CPACK_SOURCE_STRIP_FILES ...FIXME... )
#SET ( CPACK_SOURCE_IGNORE_FILES "\\.svn;\\.#;.*~" )
SET ( CPACK_SOURCE_IGNORE_FILES ".*~" )
INCLUDE ( CPack )

# TODO: equivalents of the old:
#  * make doxygen
#  * make help
#  * BOOST_DYNAMIC

# Do TIFF, JPEG, PNG actually look in external?
