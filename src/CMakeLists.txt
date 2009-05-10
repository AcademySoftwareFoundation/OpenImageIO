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

SET (CMAKE_VERBOSE_MAKEFILE on)

INCLUDE_DIRECTORIES ("include")

###########################################################################
# Platform setup
IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    SET (SYSTEM_DL_LIB dl)
ELSE ()
    SET (SYSTEM_DL_LIB)
ENDIF ()


###########################################################################
# Find libraries

setup_path (THIRD_PARTY_TOOLS_HOME 
            "${PROJECT_SOURCE_DIR}/../../external/dist/${OIIO_PLATFORM}"
            "Location of third party libraries in the external project" )


###########################################################################
# IlmBase and OpenEXR setup

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library" )
SET ( ILMBASE_VERSION 1.0.1
     CACHE STRING "Version of the ILMBase library")
SET (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
     CACHE PATH "Location of the ILMBase library install")
SET (ILMBASE_LIB_AREA "${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION}"
     CACHE PATH "Directory containing IlmBase libraries")
MARK_AS_ADVANCED (ILMBASE_VERSION)
MARK_AS_ADVANCED (ILMBASE_HOME)
MARK_AS_ADVANCED (ILMBASE_LIB_AREA)
INCLUDE_DIRECTORIES ( "${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR" )
LINK_DIRECTORIES ( "${ILMBASE_LIB_AREA}" )

MACRO ( LINK_ILMBASE_HALF target )
    TARGET_LINK_LIBRARIES ( ${target} Half )
ENDMACRO ()

MACRO ( LINK_ILMBASE target )
    TARGET_LINK_LIBRARIES ( ${target} Imath Half IlmThread Iex )
ENDMACRO ()

SET (OPENEXR_VERSION 1.6.1 CACHE STRING "OpenEXR version number")
SET (OPENEXR_VERSION_DIGITS 10601 CACHE STRING "OpenEXR version preprocessor number")
MARK_AS_ADVANCED (OPENEXR_VERSION)
MARK_AS_ADVANCED (OPENEXR_VERSION_DIGITS)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
SET (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}"
     CACHE PATH "Location of the OpenEXR library install")
SET (OPENEXR_LIB_AREA "${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION}"
     CACHE PATH "Directory containing the OpenEXR libraries")
MARK_AS_ADVANCED (OPENEXR_HOME)
MARK_AS_ADVANCED (OPENEXR_LIB_AREA)
INCLUDE_DIRECTORIES ( "${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION}" )
LINK_DIRECTORIES ( "${OPENEXR_LIB_AREA}" )
ADD_DEFINITIONS ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
SET ( OPENEXR_LIBRARIES "IlmImf" )
MACRO ( LINK_OPENEXR target )
    TARGET_LINK_LIBRARIES ( ${target} IlmImf )
ENDMACRO ()


# end IlmBase and OpenEXR setup
###########################################################################


###########################################################################
# zlib setup

SET ( CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include/zlib-1.2.3" )
SET ( CMAKE_LIBRARY_PATH "${THIRD_PARTY_TOOLS_HOME}/lib/zlib-1.2.3" )
FIND_PACKAGE ( ZLIB REQUIRED )
# end zlib setup
###########################################################################


###########################################################################
# Boost setup

MESSAGE ( STATUS "boost home ${BOOST_ROOT}" )

set(Boost_ADDITIONAL_VERSIONS "1.38" "1.38.0" "1.37" "1.37.0")
#set (Boost_USE_STATIC_LIBS   ON)
set (Boost_USE_MULTITHREADED ON)
#if (APPLE)
#    set (Boost_COMPILER xgcc42-mt)
#    set (BOOST_SUFFIX xgcc42-mt-1_38)
#else ()
#    set (Boost_COMPILER gcc42-mt)
#    set (BOOST_SUFFIX gcc42-mt-1_38)
#endif ()
find_package ( Boost 1.35 REQUIRED 
               COMPONENTS filesystem program_options regex system thread
              )

MESSAGE (STATUS "Boost found ${Boost_FOUND} ")
MESSAGE (STATUS "Boost libraries ${Boost_LIBRARIES}")
MESSAGE (STATUS "Boost include ${Boost_INCLUDE_DIRS}")
MESSAGE (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}" )
MESSAGE (STATUS "Boost compiler ${Boost_COMPILER}" )

INCLUDE_DIRECTORIES ( "${Boost_INCLUDE_DIRS}")
LINK_DIRECTORIES ( "${Boost_LIBRARY_DIRS}" )

# end Boost setup
###########################################################################

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
#ADD_SUBDIRECTORY ( libutil )
ADD_SUBDIRECTORY ( libOpenImageIO )
ADD_SUBDIRECTORY ( idiff )
ADD_SUBDIRECTORY ( iconvert )
ADD_SUBDIRECTORY ( iinfo )
ADD_SUBDIRECTORY ( igrep )
ADD_SUBDIRECTORY ( maketx )
ADD_SUBDIRECTORY ( testtex )
ADD_SUBDIRECTORY ( hdr.imageio )
ADD_SUBDIRECTORY ( png.imageio )
ADD_SUBDIRECTORY ( include )
# to do: bmp hdr ico jpeg openexr png tiff zfile
# to do: doc iv 
