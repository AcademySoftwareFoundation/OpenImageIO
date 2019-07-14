###########################################################################
# Find libraries

# When not in VERBOSE mode, try to make things as quiet as possible
if (NOT VERBOSE)
    set (Boost_FIND_QUIETLY true)
    set (DCMTK_FIND_QUIETLY true)
    set (FFmpeg_FIND_QUIETLY true)
    set (Field3D_FIND_QUIETLY true)
    set (Freetype_FIND_QUIETLY true)
    set (GIF_FIND_QUIETLY true)
    set (HDF5_FIND_QUIETLY true)
    set (IlmBase_FIND_QUIETLY true)
    set (JPEG_FIND_QUIETLY true)
    set (LIBHEIF_FIND_QUIETLY true)
    set (LibRaw_FIND_QUIETLY true)
    set (Nuke_FIND_QUIETLY true)
    set (OpenColorIO_FIND_QUIETLY true)
    set (OpenCV_FIND_QUIETLY true)
    set (OpenEXR_FIND_QUIETLY true)
    set (OpenGL_FIND_QUIETLY true)
    set (OpenJpeg_FIND_QUIETLY true)
    set (OPENVDB_FIND_QUIETLY true)
    set (PkgConfig_FIND_QUIETLY true)
    set (PNG_FIND_QUIETLY TRUE)
    set (PTex_FIND_QUIETLY TRUE)
    set (PugiXML_FIND_QUIETLY TRUE)
    set (PythonInterp_FIND_QUIETLY true)
    set (PythonLibs_FIND_QUIETLY true)
    set (Qt5_FIND_QUIETLY true)
    set (TBB_FIND_QUIETLY true)
    set (Threads_FIND_QUIETLY true)
    set (TIFF_FIND_QUIETLY true)
    set (WEBP_FIND_QUIETLY true)
    set (ZLIB_FIND_QUIETLY true)
endif ()

include (ExternalProject)

option (BUILD_MISSING_DEPS "Try to download and build any missing dependencies" OFF)


###########################################################################
# TIFF
if (NOT TIFF_LIBRARIES OR NOT TIFF_INCLUDE_DIR)
    find_package (TIFF 3.9 REQUIRED)
    include_directories (${TIFF_INCLUDE_DIR})
else ()
    message (STATUS "Custom TIFF_LIBRARIES ${TIFF_LIBRARIES}")
    message (STATUS "Custom TIFF_INCLUDE_DIR ${TIFF_INCLUDE_DIR}")
endif ()


###########################################################################
# Several packages need Zlib
find_package (ZLIB REQUIRED)


###########################################################################
# PNG
find_package (PNG REQUIRED)


###########################################################################
# IlmBase & OpenEXR setup

find_package (OpenEXR 2.0 REQUIRED)
#OpenEXR 2.2 still has problems with importing ImathInt64.h unqualified
#thus need for ilmbase/OpenEXR
include_directories ("${OPENEXR_INCLUDE_DIR}"
                     "${ILMBASE_INCLUDE_DIR}"
                     "${ILMBASE_INCLUDE_DIR}/OpenEXR")
if (NOT OpenEXR_FIND_QUIETLY)
    message (STATUS "OPENEXR_INCLUDE_DIR = ${OPENEXR_INCLUDE_DIR}")
    message (STATUS "OPENEXR_LIBRARIES = ${OPENEXR_LIBRARIES}")
endif ()


# OpenEXR setup
###########################################################################


###########################################################################
# Boost setup

if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS   ON)
endif ()
set (Boost_USE_MULTITHREADED ON)
if (BOOST_CUSTOM)
    set (Boost_FOUND true)
    # N.B. For a custom version, the caller had better set up the variables
    # Boost_VERSION, Boost_INCLUDE_DIRS, Boost_LIBRARY_DIRS, Boost_LIBRARIES.
else ()
    set (Boost_COMPONENTS filesystem system thread)
    if (NOT USE_STD_REGEX)
        list (APPEND Boost_COMPONENTS regex)
    endif ()
    # The FindBoost.cmake interface is broken if it uses boost's installed
    # cmake output (e.g. boost 1.70.0, cmake <= 3.14). Specifically it fails
    # to set the expected variables printed below. So until that's fixed
    # force FindBoost.cmake to use the original brute force path.
    set (Boost_NO_BOOST_CMAKE ON)
    find_package (Boost 1.53 REQUIRED
                  COMPONENTS ${Boost_COMPONENTS})
    if (Boost_FOUND)
        message (STATUS "Boost include      ${Boost_INCLUDE_DIR} ")
        message (STATUS "Boost lib debug    ${Boost_LIBRARY_DIR_DEBUG} ")
        message (STATUS "Boost lib release  ${Boost_LIBRARY_DIR_RELEASE} ")
        message (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
        message (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
        message (STATUS "Boost libraries    ${Boost_LIBRARIES}")
    endif ()
endif ()

# On Linux, Boost 1.55 and higher seems to need to link against -lrt
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND ${Boost_VERSION} GREATER 105499)
    list (APPEND Boost_LIBRARIES "rt")
endif ()

message (STATUS "Boost version ${Boost_VERSION}")
if (NOT Boost_FIND_QUIETLY)
    message (STATUS "Boost found        ${Boost_FOUND} ")
    message (STATUS "BOOST_ROOT         ${BOOST_ROOT}")
    message (STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
    message (STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
    message (STATUS "Boost libraries    ${Boost_LIBRARIES}")
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

    if (OCIO_FOUND)
        include_directories (${OCIO_INCLUDES})
        add_definitions ("-DUSE_OCIO=1")
    else ()
        message (STATUS "Skipping OpenColorIO support")
    endif ()

    if (LINKSTATIC)
        find_library (TINYXML_LIBRARY NAMES tinyxml)
        if (TINYXML_LIBRARY)
            set (OCIO_LIBRARIES ${OCIO_LIBRARIES} ${TINYXML_LIBRARY})
        endif ()
        find_library (YAML_LIBRARY NAMES yaml-cpp)
        if (YAML_LIBRARY)
            set (OCIO_LIBRARIES ${OCIO_LIBRARIES} ${YAML_LIBRARY})
        endif ()
        find_library (LCMS2_LIBRARY NAMES lcms2)
        if (LCMS2_LIBRARY)
            set (OCIO_LIBRARIES ${OCIO_LIBRARIES} ${LCMS2_LIBRARY})
        endif ()
    endif ()
else ()
    message (STATUS "OpenColorIO disabled")
endif ()

# end OpenColorIO setup
###########################################################################


###########################################################################
# Qt setup

if (USE_QT)
    set (qt5_modules Core Gui Widgets)
    if (USE_OPENGL)
        list (APPEND qt5_modules OpenGL)
    endif ()
    find_package (Qt5 COMPONENTS ${qt5_modules})
endif ()
if (USE_QT AND Qt5_FOUND)
    if (NOT Qt5_FIND_QUIETLY)
        message (STATUS "Qt5_FOUND=${Qt5_FOUND}")
    endif ()
else ()
    message (STATUS "No Qt5 -- skipping components that need Qt5.")
    if (USE_QT AND NOT Qt5_FOUND AND APPLE)
        message (STATUS "If you think you installed qt5 with Homebrew and it still doesn't work,")
        message (STATUS "try:   export PATH=/usr/local/opt/qt5/bin:$PATH")
    endif ()
endif ()

# end Qt setup
###########################################################################


###########################################################################
# BZIP2 - used by ffmped and freetype
find_package (BZip2)   # Used by ffmpeg
if (NOT BZIP2_FOUND)
    set (BZIP2_LIBRARIES "")
endif ()


###########################################################################
# FFmpeg

if (USE_FFMPEG)
    find_package (FFmpeg 2.6)
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
                      PATHS
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
                   "${PROJECT_SOURCE_DIR}/src/include"
                   "${FIELD3D_HOME}/include"
                  )
    endif ()
    find_library (FIELD3D_LIBRARY
                  NAMES Field3D
                  PATHS "${FIELD3D_HOME}/lib"
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


###########################################################################
# Intel TBB
if (USE_TBB)
    if (DEBUGMODE)
        set(TBB_USE_DEBUG_BUILD ON)
    endif ()
    find_package (TBB 2017)     # 2017 min for VFX Platform 2018
    if (TBB_FOUND)
        if (NOT TBB_FIND_QUIETLY)
            message (STATUS "Intel TBB includes = ${TBB_INCLUDE_DIRS}")
            message (STATUS "Intel TBB library = ${TBB_LIBRARIES}")
        endif ()
        add_definitions ("-DUSE_TBB=1")
    else ()
        message (STATUS "Intel TBB not found, TBB_ROOT_DIR='${TBB_ROOT_DIR}'")
    endif ()
endif ()

# end Intel TBB setup
###########################################################################


###########################################################################
# OpenVDB
if (USE_OPENVDB AND TBB_FOUND)
    find_package (OpenVDB 5.0)   # 5.0 min for VFX Platform 2017
    if (OPENVDB_FOUND)
        if (NOT OPENVDB_FIND_QUIETLY)
            message (STATUS "OpenVDB includes = ${OPENVDB_INCLUDE_DIRS}")
            message (STATUS "OpenVDB libraries = ${OPENVDB_LIBRARIES}")
        endif ()
        add_definitions ("-DUSE_OPENVDB=1")
    else ()
        message (STATUS "OpenVDB not found, OPENVDB_ROOT_DIR='${OPENVDB_ROOT_DIR}'")
    endif ()
else ()
    if (USE_OPENVDB AND NOT TBB_FOUND)
        set (oiio_vdb_why ", could not find Intel TBB")
    endif ()
    message (STATUS "OpenVDB will not be used${oiio_vdb_why}")
endif ()

# end OpenVDB setup
###########################################################################



###########################################################################
# JPEG

if (USE_JPEGTURBO)
    find_package (JPEGTurbo)
endif ()
if (JPEG_FOUND)
    add_definitions ("-DUSE_JPEG_TURBO=1")
    set (JPEG_TURBO_FOUND 1)
else ()
    # Try to find the non-turbo version
    find_package (JPEG REQUIRED)
endif ()
include_directories (${JPEG_INCLUDE_DIR})

# end JPEG
###########################################################################


###########################################################################
# OpenJpeg
if (USE_OPENJPEG)
    find_package (OpenJpeg)
endif()
# end OpenJpeg setup
###########################################################################


###########################################################################
# LibRaw
if (USE_LIBRAW)
    if (NOT LibRaw_FIND_QUIETLY)
        message (STATUS "Looking for LibRaw with ${LIBRAW_PATH}")
    endif ()
    find_package (LibRaw)
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

    if (LINKSTATIC)
        find_package (Jasper)
        find_library (LCMS2_LIBRARIES NAMES lcms2)
        set (LibRaw_r_LIBRARIES ${LibRaw_r_LIBRARIES} ${JASPER_LIBRARIES} ${LCMS2_LIBRARIES})
    endif ()
else ()
    message (STATUS "Not using LibRaw")
endif()

# end LibRaw setup
###########################################################################


###########################################################################
# WebP setup

if (USE_WEBP)
    if (NOT WEBP_FIND_QUIETLY)
        message (STATUS "WEBP_HOME=${WEBP_HOME}")
    endif ()
    find_path (WEBP_INCLUDE_DIR webp/encode.h
               "${PROJECT_SOURCE_DIR}/src/include"
               "${WEBP_HOME}")
    find_library (WEBP_LIBRARY
                  NAMES webp
                  PATHS "${WEBP_HOME}")
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
else ()
    message (STATUS "Not using WebP")
endif ()

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
    find_package (OpenCV)
    if (OpenCV_FOUND)
        add_definitions ("-DUSE_OPENCV")
    else ()
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
    if (NOT PTEX_FOUND)
        set (PTEX_INCLUDE_DIR "")
        set (PTEX_LIBRARIES "")
    endif ()
endif()
# end PTEX setup
###########################################################################


###########################################################################
# DCMTK
if (USE_DICOM)
    find_package (DCMTK 3.6.1)
    if (NOT DCMTK_FOUND)
        set (DCMTK_INCLUDE_DIR "")
        set (DCMTK_LIBRARIES "")
    endif ()
endif()
# end DCMTK setup
###########################################################################


###########################################################################
# HEIF
if (USE_HEIF)
    find_package (Libheif 1.3)
endif()
# end HEIF setup
###########################################################################


###########################################################################
# pybind11

option (BUILD_PYBIND11_FORCE "Force local download/build of Pybind11 even if installed" OFF)
option (BUILD_MISSING_PYBIND11 "Local download/build of Pybind11 if not installed" ON)
set (BUILD_PYBIND11_VERSION "v2.2.4" CACHE STRING "Preferred pybind11 version, of downloading/building our own")
set (PYBIND11_HOME "" CACHE STRING "Installed pybind11 location hint")
set (BUILD_PYBIND11_MINIMUM_VERSION "2.2.0")

macro (find_or_download_pybind11)
    # If we weren't told to force our own download/build of pybind11, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_PYBIND11_FORCE)
        find_path (PYBIND11_INCLUDE_DIR pybind11/pybind11.h
               "${PROJECT_SOURCE_DIR}/ext/pybind11/include"
               "${PYBIND11_HOME}"
               "$ENV{PYBIND11_HOME}"
               )
    endif ()
    # Check the version -- if it's too old, download a local copy.
    if (PYBIND11_INCLUDE_DIR)
        set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/detail/common.h")
        IF (NOT EXISTS ${PYBIND11_COMMON_FILE})
            set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/common.h")
        endif ()
        file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MAJOR .*$")
        string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MAJOR ${TMP})
        file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MINOR .*$")
        string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MINOR ${TMP})
        set (PYBIND11_VERSION "${PYBIND11_VERSION_MAJOR}.${PYBIND11_VERSION_MINOR}")
        if ("${PYBIND11_VERSION}" VERSION_LESS BUILD_PYBIND11_MINIMUM_VERSION)
            message (WARNING "pybind11 from ${PYBIND11_INCLUDE_DIR} is too old (${PYBIND11_VERSION}), minimum is ${BUILD_PYBIND11_MINIMUM_VERSION}, downloading our own.")
            set (PYBIND11_INCLUDE_DIR "")
        endif ()
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    if ((BUILD_MISSING_PYBIND11 AND NOT PYBIND11_INCLUDE_DIR) OR BUILD_PYBIND11_FORCE)
        message (STATUS "Building local Pybind11")
        set (PYBIND11_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/pybind11")
        set (PYBIND11_GIT_REPOSITORY "https://github.com/pybind/pybind11")
        if (NOT IS_DIRECTORY ${PYBIND11_INSTALL_DIR}/include)
            find_package (Git REQUIRED)
            execute_process(COMMAND
                            ${GIT_EXECUTABLE} clone ${PYBIND11_GIT_REPOSITORY}
                            --branch ${BUILD_PYBIND11_VERSION}
                            ${PYBIND11_INSTALL_DIR}
                            )
            if (IS_DIRECTORY ${PYBIND11_INSTALL_DIR}/include)
                message (STATUS "DOWNLOADED pybind11 to ${PYBIND11_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download pybind11")
            endif ()
        endif ()
        set (PYBIND11_INCLUDE_DIR "${PYBIND11_INSTALL_DIR}/include")
        set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/detail/common.h")
        IF (NOT EXISTS ${PYBIND11_COMMON_FILE})
            set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/common.h")
        endif ()
        file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MAJOR .*$")
        string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MAJOR ${TMP})
        file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MINOR .*$")
        string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MINOR ${TMP})
        set (PYBIND11_VERSION "${PYBIND11_VERSION_MAJOR}.${PYBIND11_VERSION_MINOR}")
    endif ()

    if ("${PYBIND11_VERSION}" VERSION_LESS BUILD_PYBIND11_MINIMUM_VERSION)
        message (WARNING "pybind11 from ${PYBIND11_INCLUDE_DIR} is too old (${PYBIND11_VERSION}), minimum is ${BUILD_PYBIND11_MINIMUM_VERSION}.")
        set (PYBIND11_INCLUDE_DIR "")
    endif ()
    if (PYBIND11_INCLUDE_DIR)
        message (STATUS "pybind11 ${PYBIND11_VERSION}, include dir: ${PYBIND11_INCLUDE_DIR}")
    else ()
        message (FATAL_ERROR "pybind11 is missing! If it's not on your "
                 "system, you need to install it, or build with either "
                 "-DBUILD_MISSING_DEPS=ON or -DBUILD_PYBIND11_FORCE=ON. "
                 "Or build with -DUSE_PYTHON=OFF.")
    endif ()
endmacro()

###########################################################################
# Tessil/robin-map

option (BUILD_ROBINMAP_FORCE "Force local download/build of robin-map even if installed" OFF)
option (BUILD_MISSING_ROBINMAP "Local download/build of robin-map if not installed" ON)
set (BUILD_ROBINMAP_VERSION "1e59f1993c7b6eace15032f38b5acb0bc34f530b" CACHE STRING "Preferred Tessil/robin-map version, of downloading/building our own")
set (ROBINMAP_HOME "" CACHE STRING "Installed robin-map location hint")

macro (find_or_download_robin_map)
    # If we weren't told to force our own download/build of robin-map, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_ROBINMAP_FORCE)
        find_path (ROBINMAP_INCLUDE_DIR tsl/robin_map.h
               "${PROJECT_SOURCE_DIR}/ext/robin-map/tsl/"
               "${ROBINMAP_HOME}"
               "$ENV{ROBINMAP_HOME}"
               )
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    # Download the headers from github
    if ((BUILD_MISSING_ROBINMAP AND NOT ROBINMAP_INCLUDE_DIR) OR BUILD_ROBINMAP_FORCE)
        message (STATUS "Downloading local Tessil/robin-map")
        set (ROBINMAP_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/robin-map")
        set (ROBINMAP_GIT_REPOSITORY "https://github.com/Tessil/robin-map")
        if (NOT IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/tsl)
            find_package (Git REQUIRED)
            execute_process(COMMAND             ${GIT_EXECUTABLE} clone    ${ROBINMAP_GIT_REPOSITORY} -n ${ROBINMAP_INSTALL_DIR})
            execute_process(COMMAND             ${GIT_EXECUTABLE} checkout ${BUILD_ROBINMAP_VERSION}
                            WORKING_DIRECTORY   ${ROBINMAP_INSTALL_DIR})
                if (IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/tsl)
                message (STATUS "DOWNLOADED Tessil/robin-map to ${ROBINMAP_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download Tessil/robin-map")
            endif ()
        endif ()
        set (ROBINMAP_INCLUDE_DIR "${ROBINMAP_INSTALL_DIR}")
    endif ()

    if (ROBINMAP_INCLUDE_DIR)
        message (STATUS "robin-map include dir: ${ROBINMAP_INCLUDE_DIR}")
    else ()
        message (FATAL_ERROR "Tessil/robin-map is missing!")
    endif ()
endmacro()


###########################################################################

