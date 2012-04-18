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

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library")
setup_string (ILMBASE_VERSION 1.0.1
              "Version of the ILMBase library")
mark_as_advanced (ILMBASE_VERSION)
setup_path (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the ILMBase library install")
mark_as_advanced (ILMBASE_HOME)

find_package (IlmBase REQUIRED)

include_directories ("${ILMBASE_INCLUDE_DIR}")
include_directories ("${ILMBASE_INCLUDE_DIR}/OpenEXR")

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

find_package (OpenEXR REQUIRED)

include_directories (${OPENEXR_INCLUDE_DIR})
include_directories (${OPENEXR_INCLUDE_DIR}/OpenEXR)
#add_definitions ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
macro (LINK_OPENEXR target)
    target_link_libraries (${target} ${OPENEXR_LIBRARIES})
endmacro ()


# end IlmBase and OpenEXR setup
###########################################################################

###########################################################################
# Boost setup

message (STATUS "BOOST_ROOT ${BOOST_ROOT}")

set (Boost_ADDITIONAL_VERSIONS "1.49" "1.48" "1.47" "1.46" "1.45" "1.44" 
                               "1.43" "1.43.0" "1.42" "1.42.0" 
                               "1.41" "1.41.0" "1.40" "1.40.0")
if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS   ON)
endif ()
set (Boost_USE_MULTITHREADED ON)
if (BOOST_CUSTOM)
    set (Boost_FOUND true)
    # N.B. For a custom version, the caller had better set up the variables
    # Boost_VERSION, Boost_INCLUDE_DIRS, Boost_LIBRARY_DIRS, Boost_LIBRARIES.
else ()
    find_package (Boost 1.40 REQUIRED 
                  COMPONENTS filesystem regex system thread
                 )
    # Try to figure out if this boost distro has Boost::python.  If we
    # include python in the component list above, cmake will abort if
    # it's not found.  So we resort to checking for the boost_python
    # library's existance to get a soft failure.
    find_library (oiio_boost_python_lib boost_python
                  PATHS ${Boost_LIBRARY_DIRS} NO_DEFAULT_PATH)
    mark_as_advanced (oiio_boost_python_lib)
    if (NOT oiio_boost_python_lib AND Boost_SYSTEM_LIBRARY_RELEASE)
        get_filename_component (oiio_boost_PYTHON_rel
                                ${Boost_SYSTEM_LIBRARY_RELEASE} NAME
                               )
        string (REGEX REPLACE "^(lib)?(.+)_system(.+)$" "\\2_python\\3"
                oiio_boost_PYTHON_rel ${oiio_boost_PYTHON_rel}
               )
        find_library (oiio_boost_PYTHON_LIBRARY_RELEASE
                      NAMES ${oiio_boost_PYTHON_rel} lib${oiio_boost_PYTHON_rel}
                      HINTS ${Boost_LIBRARY_DIRS}
                      NO_DEFAULT_PATH
                     )
        mark_as_advanced (oiio_boost_PYTHON_LIBRARY_RELEASE)
    endif ()
    if (NOT oiio_boost_python_lib AND Boost_SYSTEM_LIBRARY_DEBUG)
        get_filename_component (oiio_boost_PYTHON_dbg
                                ${Boost_SYSTEM_LIBRARY_DEBUG} NAME
                               )
        string (REGEX REPLACE "^(lib)?(.+)_system(.+)$" "\\2_python\\3"
                oiio_boost_PYTHON_dbg ${oiio_boost_PYTHON_dbg}
               )
        find_library (oiio_boost_PYTHON_LIBRARY_DEBUG
                      NAMES ${oiio_boost_PYTHON_dbg} lib${oiio_boost_PYTHON_dbg}
                      HINTS ${Boost_LIBRARY_DIRS}
                      NO_DEFAULT_PATH
                     )
        mark_as_advanced (oiio_boost_PYTHON_LIBRARY_DEBUG)
    endif ()
    if (oiio_boost_python_lib OR
        oiio_boost_PYTHON_LIBRARY_RELEASE OR oiio_boost_PYTHON_LIBRARY_DEBUG)
        set (oiio_boost_PYTHON_FOUND ON)
    else ()
        set (oiio_boost_PYTHON_FOUND OFF)
    endif ()
endif ()

message (STATUS "Boost found ${Boost_FOUND} ")
message (STATUS "Boost version      ${Boost_VERSION}")
message (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
message (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
message (STATUS "Boost libraries    ${Boost_LIBRARIES}")
message (STATUS "Boost_python_FOUND ${oiio_boost_PYTHON_FOUND}")
if (NOT oiio_boost_PYTHON_FOUND)
    # If Boost python components were not found, turn off all python support.
    message (STATUS "Boost python support not found -- will not build python components!")
    if (APPLE AND USE_PYTHON)
        message (STATUS "   If your Boost is from Macports, you need the +python26 variant to get Python support.")
    endif ()
    set (USE_PYTHON OFF)
    set (PYTHONLIBS_FOUND OFF)
endif ()

include_directories (SYSTEM "${Boost_INCLUDE_DIRS}")
link_directories ("${Boost_LIBRARY_DIRS}")

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    add_definitions ("-Wno-parentheses")
#endif ()

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
# OpenColorIO Setup

if (USE_OCIO)
    # If 'OCIO_PATH' not set, use the env variable of that name if available
    if (NOT OCIO_PATH)
        if (NOT $ENV{OCIO_PATH} STREQUAL "")
            set (OCIO_PATH $ENV{OCIO_PATH})
        endif ()
    endif()

    find_package (OpenColorIO)
    FindOpenColorIO ()

    if (OCIO_FOUND)
        message (STATUS "OpenColorIO enabled")
        message(STATUS "OCIO_INCLUDES: ${OCIO_INCLUDES}")
        include_directories (${OCIO_INCLUDES})
        add_definitions ("-DUSE_OCIO=1")
    else ()
        message (STATUS "Skipping OpenColorIO support")
    endif ()
else ()
    message (STATUS "OpenColorIO disabled")
endif ()

# end OpenColorIO setup
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
message (STATUS "QT_INCLUDES=${QT_INCLUDES}")
message (STATUS "QT_LIBRARIES=${QT_LIBRARIES}")

# end Qt setup
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


###########################################################################
# Field3d

if (USE_FIELD3D)
    if (HDF5_CUSTOM)
        message (STATUS "Using custom HDF5")
        set (HDF5_FOUND true)
        # N.B. For a custom version, the caller had better set up the
        # variables HDF5_INCLUDE_DIRS and HDF5_LIBRARIES.
    else ()
        message (STATUS "Looking for system HDF5")
        find_package (HDF5 COMPONENTS CXX)
    endif ()
    message (STATUS "HDF5_FOUND=${HDF5_FOUND}")
    message (STATUS "HDF5_INCLUDE_DIRS=${HDF5_INCLUDE_DIRS}")
    message (STATUS "HDF5_C_LIBRARIES=${HDF5_C_LIBRARIES}")
    message (STATUS "HDF5_CXX_LIBRARIES=${HDF5_CXX_LIBRARIES}")
    message (STATUS "HDF5_LIBRARIES=${HDF5_LIBRARIES}")
    message (STATUS "HDF5_LIBRARY_DIRS=${HDF5_LIBRARY_DIRS}")
endif ()
if (USE_FIELD3D AND HDF5_FOUND)
    message (STATUS "FIELD3D_HOME=${FIELD3D_HOME}")
    find_path (FIELD3D_INCLUDES Field3D/Field.h
               ${THIRD_PARTY_TOOLS}/include
               ${PROJECT_SOURCE_DIR}/include
               ${FIELD3D_HOME}/include
              )
    find_library (FIELD3D_LIBRARY
                  NAMES Field3D
                  PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                  ${FIELD3D_HOME}/lib
                 )
    if (FIELD3D_INCLUDES AND FIELD3D_LIBRARY)
        set (FIELD3D_FOUND TRUE)
        message (STATUS "Field3D includes = ${FIELD3D_INCLUDES}")
        message (STATUS "Field3D library = ${FIELD3D_LIBRARY}")
        add_definitions ("-DUSE_FIELD3D=1")
        include_directories ("${HDF5_INCLUDE_DIRS}")
        include_directories ("${FIELD3D_INCLUDES}")
        # link_directories ("${HDF5_INCLUDE_DIRS}")
    else ()
        message (STATUS "Field3D not found")
        add_definitions ("-UUSE_FIELD3D")
        set (FIELD3D_FOUND FALSE)
    endif ()
else ()
    add_definitions ("-UUSE_FIELD3D")
    message (STATUS "Field3d will not be used")
endif ()

# end Field3d setup
###########################################################################

# OpenJpeg
if (USE_OPENJPEG)
    find_package (OpenJpeg)
endif()
# end OpenJpeg setup_path
###########################################################################

# WebP setup
    message (STATUS "WEBP_HOME=${WEBP_HOME}")
    find_path (WEBP_INCLUDE_DIR webp/encode.h
               ${THIRD_PARTY_TOOLS}/include
               ${PROJECT_SOURCE_DIR}/include  
               ${WEBP_HOME}/)
    find_library (WEBP_LIBRARY
                  NAMES webp
                  PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                  ${WEBP_HOME}/
                 )
    if (WEBP_INCLUDE_DIR AND WEBP_LIBRARY)
        set (WEBP_FOUND TRUE)
        message (STATUS "WEBP includes = ${WEBP_INCLUDE_DIR} ")
        message (STATUS "WEBP library = ${WEBP_LIBRARY} ")
    else()
        set (WEBP_FOUND FALSE)
        message (STATUS "WebP library not found")
    endif()
# end Webp setup
###########################################################################

###########################################################################
# Pugixml setup.  Normally we just use the version bundled with oiio, but
# some linux distros are quite particular about having separate packages so we
# allow this to be overridden to use the distro-provided package if desired.
if (USE_EXTERNAL_PUGIXML)
    find_package (PugiXML REQUIRED)
    # insert include path to pugixml first, to ensure that the external
    # pugixml is found, and not the one in OIIO's include directory.
    include_directories (BEFORE ${PUGIXML_INCLUDE_DIR})
endif()


###########################################################################
# OpenCV setup

if (USE_OPENCV)
    find_path (OpenCV_INCLUDE_DIR opencv/cv.h
               ${THIRD_PARTY_TOOLS}/include
               ${PROJECT_SOURCE_DIR}/include  
               ${OpenCV_HOME}/include
               /usr/local/include
               /opt/local/include
               )
    find_library (OpenCV_LIBS
                  NAMES opencv_core
                  PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                        ${PROJECT_SOURCE_DIR}/lib
                        ${OpenCV_HOME}/lib
                        /usr/local/lib
                        /opt/local/lib
                 )
    find_library (OpenCV_LIBS_highgui
                  NAMES opencv_highgui
                  PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                        ${PROJECT_SOURCE_DIR}/lib
                        ${OpenCV_HOME}/lib
                        /usr/local/lib
                        /opt/local/lib
                 )
    set (OpenCV_LIBS "${OpenCV_LIBS} ${OpenCV_LIBS_highgui}")
    if (OpenCV_INCLUDE_DIR AND OpenCV_LIBS)
        set (OpenCV_FOUND TRUE)
        add_definitions ("-DUSE_OPENCV")
        message (STATUS "OpenCV includes = ${OpenCV_INCLUDE_DIR} ")
        message (STATUS "OpenCV libs = ${OpenCV_LIBS} ")
    else ()
        set (OpenCV_FOUND FALSE)
        message (STATUS "OpenCV library not found")
    endif ()
else ()
    message (STATUS "Not using OpenCV")
endif ()

# end OpenCV setup
###########################################################################
