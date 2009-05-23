###########################################################################
# Find libraries

setup_path (THIRD_PARTY_TOOLS_HOME 
            "${PROJECT_SOURCE_DIR}/../../external/dist/${platform}"
            "Location of third party libraries in the external project")

# Add all third party tool directories to the include and library paths so
# that they'll be correctly found by the various FIND_PACKAGE() invocations.
if (EXISTS ${THIRD_PARTY_TOOLS_HOME})
    # Detect third party tools which have been successfully built using the
    # lock files which are placed there by the external project Makefile.
    file (GLOB _external_dir_lockfiles "${THIRD_PARTY_TOOLS_HOME}/*.d")
    foreach (_dir_lockfile ${_external_dir_lockfiles})
        # Grab the tool directory_name.d
        get_filename_component (_ext_dirname ${_dir_lockfile} NAME)
        # Strip off the .d extension
        string (REGEX REPLACE "\\.d$" "" _ext_dirname ${_ext_dirname})
        set (CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include/${_ext_dirname}" ${CMAKE_INCLUDE_PATH})
        set (CMAKE_LIBRARY_PATH "${THIRD_PARTY_TOOLS_HOME}/lib/${_ext_dirname}" ${CMAKE_LIBRARY_PATH})
    endforeach ()
endif ()


###########################################################################
# IlmBase and OpenEXR setup

# TODO: Place the OpenEXR stuff into a separate FindOpenEXR.cmake module.

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library")
setup_string (ILMBASE_VERSION 1.0.1
              "Version of the ILMBase library")
setup_path (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the ILMBase library install")
setup_path (ILMBASE_INCLUDE_AREA 
            "${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}/OpenEXR" 
            "Directory containing IlmBase include files")
setup_path (ILMBASE_LIB_AREA "${ILMBASE_HOME}/lib/ilmbase-${ILMBASE_VERSION}"
            "Directory containing IlmBase libraries")
mark_as_advanced (ILMBASE_VERSION)
mark_as_advanced (ILMBASE_HOME)
mark_as_advanced (ILMBASE_INCLUDE_AREA)
mark_as_advanced (ILMBASE_LIB_AREA)
include_directories ("${ILMBASE_INCLUDE_AREA}")
link_directories ("${ILMBASE_LIB_AREA}")
setup_string (SPECIAL_COMPILE_FLAGS "" 
               "Custom compilation flags")
if (SPECIAL_COMPILE_FLAGS)
    add_definitions (${SPECIAL_COMPILE_FLAGS})
endif ()

macro (LINK_ILMBASE_HALF target)
    target_link_libraries (${target} Half)
endmacro ()

macro (LINK_ILMBASE target)
    target_link_libraries (${target} Imath Half IlmThread Iex)
endmacro ()

setup_string (OPENEXR_VERSION 1.6.1 "OpenEXR version number")
setup_string (OPENEXR_VERSION_DIGITS 010601 "OpenEXR version preprocessor number")
mark_as_advanced (OPENEXR_VERSION)
mark_as_advanced (OPENEXR_VERSION_DIGITS)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
setup_path (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the OpenEXR library install")
setup_path (OPENEXR_LIB_AREA "${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION}"
            "Directory containing the OpenEXR libraries")
mark_as_advanced (OPENEXR_HOME)
mark_as_advanced (OPENEXR_LIB_AREA)
include_directories ("${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION}/OpenEXR")
link_directories ("${OPENEXR_LIB_AREA}")
add_definitions ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
set (OPENEXR_LIBRARIES "IlmImf")
macro (LINK_OPENEXR target)
    target_link_libraries (${target} IlmImf)
endmacro ()


# end IlmBase and OpenEXR setup
###########################################################################

###########################################################################
# Boost setup

message (STATUS "BOOST_ROOT ${BOOST_ROOT}")

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
if (BOOST_CUSTOM)
    set (BOOST_FOUND true)
else ()
    find_package (Boost 1.34 REQUIRED 
                  COMPONENTS filesystem program_options regex system thread
                 )
endif ()

message (STATUS "Boost found ${Boost_FOUND} ")
message (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
message (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
message (STATUS "Boost libraries    ${Boost_LIBRARIES}")

include_directories ("${Boost_INCLUDE_DIRS}")
link_directories ("${Boost_LIBRARY_DIRS}")

# end Boost setup
###########################################################################

###########################################################################
# OpenGL setup

if (USE_OPENGL)
    find_package (OpenGL)
endif ()
message (STATUS "OPENGL_FOUND=${OPENGL_FOUND} USE_OPENGL=${USE_OPENGL}")

# end OpenGL setup
###########################################################################

###########################################################################
# Qt setup

if (USE_QT)
    if (USE_OPENGL)
        set (QT_USE_QTOPENGL true)
    endif ()
    find_package (Qt4)
endif ()
message (STATUS "QT4_FOUND=${QT4_FOUND}")

# end Qt setup
###########################################################################

###########################################################################
# Gtest (Google Test) setup

set (GTEST_VERSION 1.3.0)
find_library (GTEST_LIBRARY
              NAMES gtest
              PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/)
find_path (GTEST_INCLUDES gtest/gtest.h
           ${THIRD_PARTY_TOOLS}/include/gtest-${GTEST_VERSION})
if (GTEST_INCLUDES AND GTEST_LIBRARY)
    set (GTEST_FOUND TRUE)
    message (STATUS "Gtest includes = ${GTEST_INCLUDES}")
    message (STATUS "Gtest library = ${GTEST_LIBRARY}")
else ()
    message (STATUS "Gtest not found")
endif ()

# end Gtest setup
###########################################################################
