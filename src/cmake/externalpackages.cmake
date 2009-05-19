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
# OpenGL setup

IF ( USE_OPENGL )
    find_package ( OpenGL )
ENDIF ()
MESSAGE (STATUS "OPENGL_FOUND=${OPENGL_FOUND} USE_OPENGL=${USE_OPENGL}")

# end OpenGL setup
###########################################################################

###########################################################################
# Qt setup

IF ( USE_QT )
    IF ( USE_OPENGL )
        SET ( QT_USE_QTOPENGL true )
    ENDIF ()
    find_package ( Qt4 )
ENDIF ()
MESSAGE (STATUS "QT4_FOUND=${QT4_FOUND}")

# end Qt setup
###########################################################################

###########################################################################
# Gtest (Google Test) setup

SET ( GTEST_VERSION 1.3.0 )
find_library ( GTEST_LIBRARY
               NAMES gtest
               PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/ )
find_path ( GTEST_INCLUDES gtest/gtest.h
            ${THIRD_PARTY_TOOLS}/include/gtest-${GTEST_VERSION} )
IF (GTEST_INCLUDES AND GTEST_LIBRARY )
    SET ( GTEST_FOUND TRUE )
    MESSAGE ( STATUS "Gtest includes = ${GTEST_INCLUDES}" )
    MESSAGE ( STATUS "Gtest library = ${GTEST_LIBRARY}" )
ELSE ()
    MESSAGE ( STATUS "Gtest not found" )
ENDIF ()

# end Gtest setup
###########################################################################
