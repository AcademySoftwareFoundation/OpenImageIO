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

#SET (CMAKE_VERBOSE_MAKEFILE on)

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
            "${PROJECT_SOURCE_DIR}/../../external/dist/${platform}"
            "Location of third party libraries in the external project" )

# Add all third party tool directories to the include and library paths so
# that they'll be correctly found by the various FIND_PACKAGE() invocations.
IF ( EXISTS ${THIRD_PARTY_TOOLS_HOME} )
    # Detect third party tools which have been successfully built using the
    # lock files which are placed there by the external project Makefile.
    FILE ( GLOB _external_dir_lockfiles "${THIRD_PARTY_TOOLS_HOME}/*.d" )
    FOREACH ( _dir_lockfile ${_external_dir_lockfiles} )
        # Grab the tool directory_name.d
        GET_FILENAME_COMPONENT ( _ext_dirname ${_dir_lockfile} NAME )
        # Strip off the .d extension
        STRING ( REGEX REPLACE "\\.d$" "" _ext_dirname ${_ext_dirname} )
        SET ( CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include/${_ext_dirname}" ${CMAKE_INCLUDE_PATH} )
        SET ( CMAKE_LIBRARY_PATH "${THIRD_PARTY_TOOLS_HOME}/lib/${_ext_dirname}" ${CMAKE_LIBRARY_PATH} )
    ENDFOREACH ()
ENDIF ()


###########################################################################
# IlmBase and OpenEXR setup

# TODO: Place the OpenEXR stuff into a separate FindOpenEXR.cmake module.

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library" )
setup_string ( ILMBASE_VERSION 1.0.1
               "Version of the ILMBase library")
setup_path ( ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
             "Location of the ILMBase library install")
setup_path ( ILMBASE_INCLUDE_AREA 
             "${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR" 
             "Directory containing IlmBase include files" )
setup_path ( ILMBASE_LIB_AREA "${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION}"
             "Directory containing IlmBase libraries")
MARK_AS_ADVANCED (ILMBASE_VERSION)
MARK_AS_ADVANCED (ILMBASE_HOME)
MARK_AS_ADVANCED (ILMBASE_INCLUDE_AREA)
MARK_AS_ADVANCED (ILMBASE_LIB_AREA)
INCLUDE_DIRECTORIES ( "${ILMBASE_INCLUDE_AREA}" )
LINK_DIRECTORIES ( "${ILMBASE_LIB_AREA}" )
setup_string ( SPECIAL_COMPILE_FLAGS "" 
               "Custom compilation flags" )
IF ( SPECIAL_COMPILE_FLAGS )
    ADD_DEFINITIONS ( ${SPECIAL_COMPILE_FLAGS} )
ENDIF ()

MACRO ( LINK_ILMBASE_HALF target )
    TARGET_LINK_LIBRARIES ( ${target} Half )
ENDMACRO ()

MACRO ( LINK_ILMBASE target )
    TARGET_LINK_LIBRARIES ( ${target} Imath Half IlmThread Iex )
ENDMACRO ()

setup_string (OPENEXR_VERSION 1.6.1 "OpenEXR version number")
setup_string (OPENEXR_VERSION_DIGITS 010601 "OpenEXR version preprocessor number")
MARK_AS_ADVANCED (OPENEXR_VERSION)
MARK_AS_ADVANCED (OPENEXR_VERSION_DIGITS)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
setup_path (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the OpenEXR library install")
setup_path (OPENEXR_LIB_AREA "${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION}"
            "Directory containing the OpenEXR libraries")
MARK_AS_ADVANCED (OPENEXR_HOME)
MARK_AS_ADVANCED (OPENEXR_LIB_AREA)
INCLUDE_DIRECTORIES ( "${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION}/OpenEXR" )
LINK_DIRECTORIES ( "${OPENEXR_LIB_AREA}" )
ADD_DEFINITIONS ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
SET ( OPENEXR_LIBRARIES "IlmImf" )
MACRO ( LINK_OPENEXR target )
    TARGET_LINK_LIBRARIES ( ${target} IlmImf )
ENDMACRO ()


# end IlmBase and OpenEXR setup
###########################################################################

###########################################################################
# Boost setup

MESSAGE ( STATUS "BOOST_ROOT ${BOOST_ROOT}" )

set(Boost_ADDITIONAL_VERSIONS "1.38" "1.38.0" "1.37" "1.37.0" "1.34.1" "1_34_1")
#set (Boost_USE_STATIC_LIBS   ON)
set (Boost_USE_MULTITHREADED ON)
#if (APPLE)
#    set (Boost_COMPILER xgcc42-mt)
#    set (BOOST_SUFFIX xgcc42-mt-1_38)
#else ()
#    set (Boost_COMPILER gcc42-mt)
#    set (BOOST_SUFFIX gcc42-mt-1_38)
#endif ()
IF ( BOOST_CUSTOM )
    SET (BOOST_FOUND true)
ELSE ()
    find_package ( Boost 1.34 REQUIRED 
                   COMPONENTS filesystem program_options regex system thread
                 )
ENDIF ()

MESSAGE (STATUS "Boost found ${Boost_FOUND} ")
MESSAGE (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
MESSAGE (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}" )
MESSAGE (STATUS "Boost libraries    ${Boost_LIBRARIES}")

INCLUDE_DIRECTORIES ( "${Boost_INCLUDE_DIRS}")
LINK_DIRECTORIES ( "${Boost_LIBRARY_DIRS}" )

# end Boost setup
###########################################################################

###########################################################################
# Qt setup

SET ( QT_USE_QTOPENGL true )
find_package ( Qt4 )

# end Qt setup
###########################################################################

###########################################################################
# OpenGL setup

find_package ( OpenGL )

# end OpenGL setup
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
