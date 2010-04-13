###########################################################################
# Find libraries

setup_path (THIRD_PARTY_TOOLS_HOME 
#            "${PROJECT_SOURCE_DIR}/../../external/dist/${platform}"
            "unknown"
            "Location of third party libraries in the external project")

# Add all third party tool directories to the include and library paths so
# that they'll be correctly found by the various FIND_PACKAGE() invocations.
if (THIRD_PARTY_TOOLS_HOME AND EXISTS ${THIRD_PARTY_TOOLS_HOME})
    set (CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include" ${CMAKE_INCLUDE_PATH})
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


setup_string (SPECIAL_COMPILE_FLAGS "" 
               "Custom compilation flags")
if (SPECIAL_COMPILE_FLAGS)
    add_definitions (${SPECIAL_COMPILE_FLAGS})
endif ()



###########################################################################
# IlmBase and OpenEXR setup

# TODO: Place the OpenEXR stuff into a separate FindOpenEXR.cmake module.

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library")
setup_string (ILMBASE_VERSION 1.0.1
              "Version of the ILMBase library")
mark_as_advanced (ILMBASE_VERSION)
setup_path (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the ILMBase library install")
mark_as_advanced (ILMBASE_HOME)
find_path (ILMBASE_INCLUDE_AREA OpenEXR/half.h
           ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/include
           ${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}
           ${ILMBASE_HOME}/include
          )
if (ILMBASE_CUSTOM)
    set (IlmBase_Libraries ${ILMBASE_CUSTOM_LIBRARIES})
    separate_arguments(IlmBase_Libraries)
else ()
    set (IlmBase_Libraries Imath Half IlmThread Iex)
endif ()

foreach (_lib ${IlmBase_Libraries})
    find_library (current_lib ${_lib}
                  PATHS ${ILMBASE_HOME}/lib ${ILMBASE_HOME}/lib64
                        ${ILMBASE_LIB_AREA}
                        ${ILMBASE_HOME}/ilmbase-${ILMBASE_VERSION}/lib
                  )
    list(APPEND ILMBASE_LIBRARIES ${current_lib})
endforeach ()
message (STATUS "ILMBASE_INCLUDE_AREA = ${ILMBASE_INCLUDE_AREA}")
message (STATUS "ILMBASE_LIBRARIES = ${ILMBASE_LIBRARIES}")
if (ILMBASE_INCLUDE_AREA AND ILMBASE_LIBRARIES)
    set (ILMBASE_FOUND true)
    include_directories ("${ILMBASE_INCLUDE_AREA}")
    include_directories ("${ILMBASE_INCLUDE_AREA}/OpenEXR")
else ()
    message (FATAL_ERROR "ILMBASE not found!")
endif ()

macro (LINK_ILMBASE target)
    target_link_libraries (${target} ${ILMBASE_LIBRARIES})
endmacro ()

setup_string (OPENEXR_VERSION 1.6.1 "OpenEXR version number")
mark_as_advanced (OPENEXR_VERSION)
#setup_string (OPENEXR_VERSION_DIGITS 010601 "OpenEXR version preprocessor number")
#mark_as_advanced (OPENEXR_VERSION_DIGITS)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
setup_path (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the OpenEXR library install")
mark_as_advanced (OPENEXR_HOME)
find_path (OPENEXR_INCLUDE_AREA OpenEXR/OpenEXRConfig.h
           ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/include
           ${OPENEXR_HOME}/include
           ${ILMBASE_HOME}/include/openexr-${OPENEXR_VERSION}
            )

if (OPENEXR_CUSTOM)
    set (OpenExr_Library ${OPENEXR_CUSTOM_LIBRARY})
else ()
    set (OpenExr_Library IlmImf)
endif ()
    
find_library (OPENEXR_LIBRARY ${OpenExr_Library}
                  PATHS ${OPENEXR_HOME}/lib
                        ${OPENEXR_HOME}/lib64
                        ${OPENEXR_LIB_AREA}
                        ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/lib
                 )

message (STATUS "OPENEXR_INCLUDE_AREA = ${OPENEXR_INCLUDE_AREA}")
message (STATUS "OPENEXR_LIBRARY = ${OPENEXR_LIBRARY}")
if (OPENEXR_INCLUDE_AREA AND OPENEXR_LIBRARY)
    set (OPENEXR_FOUND true)
    include_directories (${OPENEXR_INCLUDE_AREA})
    include_directories (${OPENEXR_INCLUDE_AREA}/OpenEXR)
else ()
    message (STATUS "OPENEXR not found!")
endif ()
#add_definitions ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
find_package (ZLIB)
macro (LINK_OPENEXR target)
    target_link_libraries (${target} ${OPENEXR_LIBRARY} ${ZLIB_LIBRARIES})
endmacro ()


# end IlmBase and OpenEXR setup
###########################################################################

###########################################################################
# Boost setup

message (STATUS "BOOST_ROOT ${BOOST_ROOT}")

set (Boost_ADDITIONAL_VERSIONS "1.40" "1.39" "1.38" "1.38.0" "1.37" "1.37.0" "1.34.1" "1_34_1")
if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS   ON)
endif ()
set (Boost_USE_MULTITHREADED ON)
if (BOOST_CUSTOM)
    set (Boost_FOUND true)
else ()
    find_package (Boost 1.34 REQUIRED 
                  COMPONENTS filesystem regex system thread
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
              PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                    ${THIRD_PARTY_TOOLS_HOME}/gtest-${GTEST_VERSION}/lib)
find_path (GTEST_INCLUDES gtest/gtest.h
           ${THIRD_PARTY_TOOLS}/include/gtest-${GTEST_VERSION}
           ${THIRD_PARTY_TOOLS_HOME}/gtest-${GTEST_VERSION}/include)
if (GTEST_INCLUDES AND GTEST_LIBRARY)
    set (GTEST_FOUND TRUE)
    message (STATUS "Gtest includes = ${GTEST_INCLUDES}")
    message (STATUS "Gtest library = ${GTEST_LIBRARY}")
else ()
    message (STATUS "Gtest not found")
endif ()

# end Gtest setup
###########################################################################

###########################################################################
# TBB (Intel Thread Building Blocks) setup

setup_path (TBB_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the TBB library install")
mark_as_advanced (TBB_HOME)
if (USE_TBB)
    set (TBB_VERSION 22_004oss)
    if (MSVC)
        find_library (TBB_LIBRARY
                      NAMES tbb
                      PATHS ${TBB_HOME}/lib
                      PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                      ${TBB_HOME}/tbb-${TBB_VERSION}/lib/
                     )
        find_library (TBB_DEBUG_LIBRARY
                      NAMES tbb_debug
                      PATHS ${TBB_HOME}/lib
                      PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                      ${TBB_HOME}/tbb-${TBB_VERSION}/lib/)
    endif (MSVC)
    find_path (TBB_INCLUDES tbb/tbb_stddef.h
               ${TBB_HOME}/include/tbb${TBB_VERSION}
               ${THIRD_PARTY_TOOLS}/include/tbb${TBB_VERSION}
               ${PROJECT_SOURCE_DIR}/include
              )
    if (TBB_INCLUDES OR TBB_LIBRARY)
        set (TBB_FOUND TRUE)
        message (STATUS "TBB includes = ${TBB_INCLUDES}")
        message (STATUS "TBB library = ${TBB_LIBRARY}")
        add_definitions ("-DUSE_TBB=1")
    else ()
        message (STATUS "TBB not found")
    endif ()
else ()
    add_definitions ("-DUSE_TBB=0")
    message (STATUS "TBB will not be used")
endif ()

# end TBB setup
###########################################################################

###########################################################################
# GL Extension Wrangler library setup

if (USE_OPENGL)
    set (GLEW_VERSION 1.5.1)
    find_library (GLEW_LIBRARIES
                  NAMES GLEW)
    find_path (GLEW_INCLUDES
               NAMES glew.h
               PATH_SUFFIXES GL)
    if (GLEW_INCLUDES AND GLEW_LIBRARIES)
        set (GLEW_FOUND TRUE)
        message (STATUS "GLEW includes = ${GLEW_INCLUDES}")
        message (STATUS "GLEW library = ${GLEW_LIBRARIES}")
    else ()
        message (STATUS "GLEW not found")
    endif ()
endif (USE_OPENGL)

# end GL Extension Wrangler library setup
###########################################################################
