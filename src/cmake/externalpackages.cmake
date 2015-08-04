###########################################################################
# Find libraries

# When not in VERBOSE mode, try to make things as quiet as possible
if (NOT VERBOSE)
    set (Boost_FIND_QUIETLY true)
    set (FFmpeg_FIND_QUIETLY true)
    set (Field3D_FIND_QUIETLY true)
    set (Freetype_FIND_QUIETLY true)
    set (GIF_FIND_QUIETLY true)
    set (GLEW_FIND_QUIETLY true)
    set (HDF5_FIND_QUIETLY true)
    set (IlmBase_FIND_QUIETLY true)
    set (JPEG_FIND_QUIETLY true)
    set (LibRaw_FIND_QUIETLY true)
    set (Nuke_FIND_QUIETLY true)
    set (OpenColorIO_FIND_QUIETLY true)
    set (OpenCV_FIND_QUIETLY true)
    set (OpenEXR_FIND_QUIETLY true)
    set (OpenGL_FIND_QUIETLY true)
    set (OpenJpeg_FIND_QUIETLY true)
    set (PkgConfig_FIND_QUIETLY true)
    set (PNG_FIND_QUIETLY TRUE)
    set (PTex_FIND_QUIETLY TRUE)
    set (PugiXML_FIND_QUIETLY TRUE)
    set (PythonInterp_FIND_QUIETLY true)
    set (PythonLibs_FIND_QUIETLY true)
    set (Qt4_FIND_QUIETLY true)
    set (Threads_FIND_QUIETLY true)
    set (TIFF_FIND_QUIETLY true)
    set (WEBP_FIND_QUIETLY true)
    set (ZLIB_FIND_QUIETLY true)
endif ()


setup_path (THIRD_PARTY_TOOLS_HOME
            "unknown"
            "Location of third party libraries in the external project")

# Add all third party tool directories to the include and library paths so
# that they'll be correctly found by the various FIND_PACKAGE() invocations.
if (THIRD_PARTY_TOOLS_HOME AND EXISTS "${THIRD_PARTY_TOOLS_HOME}")
    set (CMAKE_INCLUDE_PATH "${THIRD_PARTY_TOOLS_HOME}/include" "${CMAKE_INCLUDE_PATH}")
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
# IlmBase setup

find_package (IlmBase REQUIRED)

include_directories ("${ILMBASE_INCLUDE_DIR}")
include_directories ("${ILMBASE_INCLUDE_DIR}/OpenEXR")

macro (LINK_ILMBASE target)
    target_link_libraries (${target} ${ILMBASE_LIBRARIES})
endmacro ()

# end IlmBase setup
###########################################################################


###########################################################################
# OpenEXR setup

find_package (OpenEXR REQUIRED)

if (EXISTS "${OPENEXR_INCLUDE_DIR}/OpenEXR/ImfMultiPartInputFile.h")
    add_definitions (-DUSE_OPENEXR_VERSION2=1)
    setup_string (OPENEXR_VERSION 2.0.0 "OpenEXR version number")
    if (NOT OpenEXR_FIND_QUIETLY)
        message (STATUS "OpenEXR version 2.x")
    endif ()
else ()
    setup_string (OPENEXR_VERSION 1.6.1 "OpenEXR version number")
    if (NOT OpenEXR_FIND_QUIETLY)
        message (STATUS "OpenEXR version 1.x")
    endif ()
endif ()
mark_as_advanced (OPENEXR_VERSION)

include_directories ("${OPENEXR_INCLUDE_DIR}")
# OpenEXR 1.x had weird #include dirctives, this is also necessary:
include_directories ("${OPENEXR_INCLUDE_DIR}/OpenEXR")

macro (LINK_OPENEXR target)
    target_link_libraries (${target} ${OPENEXR_LIBRARIES})
endmacro ()

# OpenEXR setup
###########################################################################


###########################################################################
# Boost setup

if (NOT Boost_FIND_QUIETLY)
    message (STATUS "BOOST_ROOT ${BOOST_ROOT}")
endif ()

if (NOT DEFINED Boost_ADDITIONAL_VERSIONS)
  set (Boost_ADDITIONAL_VERSIONS "1.57" "1.56"
                                 "1.55" "1.54" "1.53" "1.52" "1.51" "1.50"
                                 "1.49" "1.48" "1.47" "1.46" "1.45" "1.44"
                                 "1.43" "1.43.0" "1.42" "1.42.0")
endif ()
if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS   ON)
endif ()
set (Boost_USE_MULTITHREADED ON)
if (BOOST_CUSTOM)
    set (Boost_FOUND true)
    # N.B. For a custom version, the caller had better set up the variables
    # Boost_VERSION, Boost_INCLUDE_DIRS, Boost_LIBRARY_DIRS, Boost_LIBRARIES.
else ()
    set (Boost_COMPONENTS filesystem regex system thread)
    find_package (Boost 1.42 REQUIRED
                  COMPONENTS ${Boost_COMPONENTS}
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

# On Linux, Boost 1.55 and higher seems to need to link against -lrt
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND ${Boost_VERSION} GREATER 105499)
    list (APPEND Boost_LIBRARIES "rt")
endif ()

if (NOT Boost_FIND_QUIETLY)
    message (STATUS "BOOST_ROOT ${BOOST_ROOT}")
    message (STATUS "Boost found ${Boost_FOUND} ")
    message (STATUS "Boost version      ${Boost_VERSION}")
    message (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
    message (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
    message (STATUS "Boost libraries    ${Boost_LIBRARIES}")
    message (STATUS "Boost_python_FOUND ${oiio_boost_PYTHON_FOUND}")
endif ()
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

# end Boost setup
###########################################################################

###########################################################################
# OpenGL setup

if (USE_OPENGL)
    find_package (OpenGL)
    if (NOT OpenGL_FIND_QUIETLY)
        message (STATUS "OPENGL_FOUND=${OPENGL_FOUND} USE_OPENGL=${USE_OPENGL}")
    endif ()
endif ()

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
        if (NOT OpenColorIO_FIND_QUIETLY)
            message (STATUS "OpenColorIO enabled")
            message(STATUS "OCIO_INCLUDES: ${OCIO_INCLUDES}")
        endif ()
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
if (USE_QT AND QT4_FOUND)
    if (NOT Qt4_FIND_QUIETLY)
        message (STATUS "QT4_FOUND=${QT4_FOUND}")
        message (STATUS "QT_INCLUDES=${QT_INCLUDES}")
        message (STATUS "QT_LIBRARIES=${QT_LIBRARIES}")
    endif ()
else ()
    message (STATUS "No Qt4 -- skipping components that need Qt4.")
endif ()

# end Qt setup
###########################################################################

###########################################################################
# GL Extension Wrangler library setup

if (USE_OPENGL)
    set (GLEW_VERSION 1.5.1)
    find_library (GLEW_LIBRARIES
                  NAMES GLEW glew32)
    find_path (GLEW_INCLUDES
               NAMES glew.h
               PATH_SUFFIXES GL)
    if (GLEW_INCLUDES AND GLEW_LIBRARIES)
        set (GLEW_FOUND TRUE)
        if (NOT GLEW_FIND_QUIETLY)
            message (STATUS "GLEW includes = ${GLEW_INCLUDES}")
            message (STATUS "GLEW library = ${GLEW_LIBRARIES}")
        endif ()
    else ()
        message (STATUS "GLEW not found")
    endif ()
else ()
    message (STATUS "USE_OPENGL=0, skipping components that need OpenGL")
endif (USE_OPENGL)

# end GL Extension Wrangler library setup
###########################################################################


###########################################################################
# FFmpeg

if (USE_FFMPEG)
    find_package (FFmpeg)
    if (FFMPEG_INCLUDE_DIR AND FFMPEG_LIBRARIES)
        set (FFMPEG_FOUND TRUE)
        if (NOT FFmpeg_FIND_QUIETLY)
            message (STATUS "FFMPEG includes = ${FFMPEG_INCLUDE_DIR}")
            message (STATUS "FFMPEG library = ${FFMPEG_LIBRARIES}")
        endif ()
    else ()
        message (STATUS "FFMPEG not found")
    endif ()
endif()

# end FFmpeg setup
###########################################################################


###########################################################################
# Field3d

if (USE_FIELD3D)
    if (HDF5_CUSTOM)
        if (NOT HDF5_FIND_QUIETLY)
            message (STATUS "Using custom HDF5")
        endif ()
        set (HDF5_FOUND true)
        # N.B. For a custom version, the caller had better set up the
        # variables HDF5_INCLUDE_DIRS and HDF5_LIBRARIES.
    else ()
        find_library (HDF5_LIBRARIES
                      NAMES hdf5
                      PATHS "${THIRD_PARTY_TOOLS_HOME}/lib/"
                      /usr/local/lib
                      /opt/local/lib
                     )
        if (HDF5_LIBRARIES)
            set (HDF5_FOUND true)
        endif ()
    endif ()
    if (NOT HDF5_FIND_QUIETLY)
        message (STATUS "HDF5_FOUND=${HDF5_FOUND}")
        message (STATUS "HDF5_LIBRARIES=${HDF5_LIBRARIES}")
    endif ()
endif ()
if (USE_FIELD3D AND HDF5_FOUND)
    if (NOT Field3D_FIND_QUIETLY)
        message (STATUS "FIELD3D_HOME=${FIELD3D_HOME}")
    endif ()
    if (FIELD3D_HOME)
        set (FIELD3D_INCLUDES "${FIELD3D_HOME}/include")
    else ()
        find_path (FIELD3D_INCLUDES Field3D/Field.h
                   "${THIRD_PARTY_TOOLS}/include"
                   "${PROJECT_SOURCE_DIR}/src/include"
                   "${FIELD3D_HOME}/include"
                  )
    endif ()
    find_library (FIELD3D_LIBRARY
                  NAMES Field3D
                  PATHS "${THIRD_PARTY_TOOLS_HOME}/lib/"
                        "${FIELD3D_HOME}/lib"
                 )
    if (FIELD3D_INCLUDES AND FIELD3D_LIBRARY)
        set (FIELD3D_FOUND TRUE)
        if (NOT Field3D_FIND_QUIETLY)
            message (STATUS "Field3D includes = ${FIELD3D_INCLUDES}")
            message (STATUS "Field3D library = ${FIELD3D_LIBRARY}")
        endif ()
        add_definitions ("-DUSE_FIELD3D=1")
        include_directories ("${FIELD3D_INCLUDES}")
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


###########################################################################
# LibRaw
if (USE_LIBRAW)
    if (NOT LibRaw_FIND_QUIETLY)
        message (STATUS "Looking for LibRaw with ${LIBRAW_PATH}")
    endif ()
    if (LIBRAW_PATH)
        # Customized path requested, don't use find_package
        FIND_PATH(LibRaw_INCLUDE_DIR libraw/libraw.h
                  PATHS "${LIBRAW_PATH}/include"
                  NO_DEFAULT_PATH
                 )
        FIND_LIBRARY(LibRaw_r_LIBRARIES NAMES raw_r
                     PATHS "${LIBRAW_PATH}/lib"
                     NO_DEFAULT_PATH
                    )
    else ()
    	find_package (LibRaw)
    endif ()
	if (LibRaw_r_LIBRARIES AND LibRaw_INCLUDE_DIR)
		set (LIBRAW_FOUND TRUE)
		include_directories (${LibRaw_INCLUDE_DIR})
        if (NOT LibRaw_FIND_QUIETLY)
            message (STATUS "Found LibRaw, include ${LibRaw_INCLUDE_DIR}")
        endif ()
	else ()
		set (LIBRAW_FOUND FALSE)
        message (STATUS "LibRaw not found!")
	endif()
else ()
    message (STATUS "Not using LibRaw")
endif()

# end LibRaw setup
###########################################################################


###########################################################################
# WebP setup

if (NOT WEBP_FIND_QUIETLY)
    message (STATUS "WEBP_HOME=${WEBP_HOME}")
endif ()
find_path (WEBP_INCLUDE_DIR webp/encode.h
           "${THIRD_PARTY_TOOLS}/include"
           "${PROJECT_SOURCE_DIR}/src/include"
           "${WEBP_HOME}")
find_library (WEBP_LIBRARY
              NAMES webp
              PATHS "${THIRD_PARTY_TOOLS_HOME}/lib/"
              "${WEBP_HOME}"
             )
if (WEBP_INCLUDE_DIR AND WEBP_LIBRARY)
    set (WEBP_FOUND TRUE)
    if (NOT WEBP_FIND_QUIETLY)
        message (STATUS "WEBP includes = ${WEBP_INCLUDE_DIR} ")
        message (STATUS "WEBP library = ${WEBP_LIBRARY} ")
    endif ()
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
    add_definitions ("-DUSE_EXTERNAL_PUGIXML=1")
endif()


###########################################################################
# OpenCV setup

if (USE_OPENCV)
    find_path (OpenCV_INCLUDE_DIR opencv/cv.h
               "${THIRD_PARTY_TOOLS}/include"
               "${PROJECT_SOURCE_DIR}/include"
               "${OpenCV_HOME}/include"
               /usr/local/include
               /opt/local/include
               )
    find_library (OpenCV_LIBS
                  NAMES opencv_core
                  PATHS "${THIRD_PARTY_TOOLS_HOME}/lib/"
                        "${PROJECT_SOURCE_DIR}/lib"
                        "${OpenCV_HOME}/lib"
                        /usr/local/lib
                        /opt/local/lib
                 )
    find_library (OpenCV_LIBS_highgui
                  NAMES opencv_highgui
                  PATHS "${THIRD_PARTY_TOOLS_HOME}/lib/"
                        "${PROJECT_SOURCE_DIR}/lib"
                        "${OpenCV_HOME}/lib"
                        /usr/local/lib
                        /opt/local/lib
                 )
    set (OpenCV_LIBS "${OpenCV_LIBS} ${OpenCV_LIBS_highgui}")
    if (OpenCV_INCLUDE_DIR AND OpenCV_LIBS)
        set (OpenCV_FOUND TRUE)
        add_definitions ("-DUSE_OPENCV")
        if (NOT OpenCV_FIND_QUIETLY)
            message (STATUS "OpenCV includes = ${OpenCV_INCLUDE_DIR} ")
            message (STATUS "OpenCV libs = ${OpenCV_LIBS} ")
        endif ()
    else ()
        set (OpenCV_FOUND FALSE)
        message (STATUS "OpenCV library not found")
    endif ()
else ()
    message (STATUS "Not using OpenCV")
endif ()

# end OpenCV setup
###########################################################################


###########################################################################
# Freetype setup

if (USE_FREETYPE)
    find_package (Freetype)
    if (FREETYPE_FOUND)
        add_definitions ("-DUSE_FREETYPE")
        if (NOT Freetype_FIND_QUIETLY)
            message (STATUS "Freetype includes = ${FREETYPE_INCLUDE_DIRS} ")
            message (STATUS "Freetype libs = ${FREETYPE_LIBRARIES} ")
        endif ()
    else ()
        message (STATUS "Freetype library not found")
    endif ()
else ()
    message (STATUS "Not using Freetype")
endif ()

# end Freetype setup
###########################################################################


###########################################################################
# OpenSSL Setup

if (USE_OPENSSL)
    find_package (OpenSSL)
    if (OPENSSL_FOUND)
        if (NOT OpenSSL_FIND_QUIETLY)
            message (STATUS "OpenSSL enabled")
            message(STATUS "OPENSSL_INCLUDES: ${OPENSSL_INCLUDE_DIR}")
        endif ()
        include_directories (${OPENSSL_INCLUDE_DIR})
        add_definitions ("-DUSE_OPENSSL=1")
    else ()
        message (STATUS "Skipping OpenSSL support")
    endif ()
else ()
    message (STATUS "OpenSSL disabled")
endif ()

# end OpenSSL setup
###########################################################################


###########################################################################
# GIF
if (USE_GIF)
    find_package (GIF)
endif()
# end GIF setup
###########################################################################


###########################################################################
# PTex
if (USE_PTEX)
    find_package (PTex)
    if (PTEX_FOUND)
        if (VERBOSE)
            message (STATUS "PTex include ${PTEX_INCLUDE_DIR}")
            message (STATUS "PTex library ${PTEX_LIBRARY}")
        endif ()
    else ()
        if (VERBOSE)
            message (STATUS "PTex not found externally, using embedded source")
        endif ()
        set (PTEX_LIBRARY "")
        set (PTEX_INCLUDE_DIR "")
    endif ()
endif()
# end PTEX setup
###########################################################################

