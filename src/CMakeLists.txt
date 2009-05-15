PROJECT (OIIO)

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
SET (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

IF ( NOT CMAKE_BUILD_TYPE ) 
#    SET( CMAKE_BUILD_TYPE "RelWithDebInfo" ) 
    SET( CMAKE_BUILD_TYPE "Release" ) 
ENDIF ()

# set default install prefix to project root directory 
#IF ( CMAKE_INSTALL_PREFIX STREQUAL "/usr/local" ) 
#    SET ( CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/dist" ) 
#ENDIF ( CMAKE_INSTALL_PREFIX STREQUAL "/usr/local" ) 

MESSAGE ( STATUS "Project source dir = ${PROJECT_SOURCE_DIR}" )
SET(CMAKE_MODULE_PATH
	"${PROJECT_SOURCE_DIR}/cmake/modules"
	"${PROJECT_SOURCE_DIR}/cmake")

INCLUDE ( util_macros )
INCLUDE ( platform )
INCLUDE ( externalpackages )

#SET (CMAKE_VERBOSE_MAKEFILE on)

INCLUDE_DIRECTORIES ("include")



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
ADD_SUBDIRECTORY ( testtex )
ADD_SUBDIRECTORY ( iv )

# Add IO plugin directories
ADD_SUBDIRECTORY ( bmp.imageio )
ADD_SUBDIRECTORY ( hdr.imageio )
ADD_SUBDIRECTORY ( ico.imageio )
ADD_SUBDIRECTORY ( jpeg.imageio )
ADD_SUBDIRECTORY ( openexr.imageio )
ADD_SUBDIRECTORY ( png.imageio )
ADD_SUBDIRECTORY ( tiff.imageio )
ADD_SUBDIRECTORY ( zfile.imageio )

ADD_SUBDIRECTORY ( include )
# TODO: doc

# Other TODO:
#  * test_libOpenImageIO
#  * testsuite

# TODO: equivalents of the old:
#  * make SHOWCOMMANDS=1
#  * make EMBEDPLUGINS=1
#  * make USE_OPENGL=0
#  * make USE_QT=0
#  * make USE_GTEST=0
#  * make debug, profile
#  * make test, code_tests, suite_tests
#  * make doxygen
#  * make help
