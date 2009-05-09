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

#SET (CMAKE_VERBOSE_MAKEFILE on)

INCLUDE_DIRECTORIES ("include")


MACRO (setup_var name defaultval)
    # If the named variable already has a value (was set by -D...), leave
    # it alone.  But if it's not yet set...
    IF ( "${${name}}" STREQUAL "" )
        # If there's an environment variable of the same name that's
        # nonempty, use the env variable.  Otherwise, use the default.
        IF ( NOT $ENV{${name}} STREQUAL "" )
            SET ( ${name} $ENV{${name}} )
        ELSE ()
            SET ( ${name} ${defaultval} )
        ENDIF ()
    ENDIF ()
    MESSAGE ( STATUS "${name} = ${${name}}" )
ENDMACRO ()


setup_var ( THIRD_PARTY_TOOLS_HOME ${PROJECT_SOURCE_DIR}/../external/dist/${OIIO_PLATFORM} )



###########################################################################
# IlmBase and OpenEXR setup

setup_var (ILMBASE_VERSION 1.0.1)
setup_var (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}")
setup_var (ILMBASE_LIB_AREA "${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION}")
INCLUDE_DIRECTORIES ( "${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR" )
LINK_DIRECTORIES ( "${ILMBASE_LIB_AREA}" )

MACRO ( LINK_ILMBASE_HALF target )
    TARGET_LINK_LIBRARIES ( ${target} Half )
ENDMACRO ()

MACRO ( LINK_ILMBASE target )
    TARGET_LINK_LIBRARIES ( ${target} Imath Half IlmThread Iex )
ENDMACRO ()

setup_var (OPENEXR_VERSION 1.6.1)
setup_var (OPENEXR_VERSION_DIGITS 10601)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
setup_var (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}")
setup_var (OPENEXR_LIB_AREA "${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION}")
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
# Boost setup

MESSAGE ( STATUS "boost home ${BOOST_ROOT}" )

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
ADD_SUBDIRECTORY ( include )
# to do: bmp hdr ico jpeg openexr png tiff zfile
# to do: doc iv 
