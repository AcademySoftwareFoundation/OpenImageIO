# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

###########################################################################
# Find external dependencies
###########################################################################

if (NOT VERBOSE)
    set (PkgConfig_FIND_QUIETLY true)
    set (Threads_FIND_QUIETLY true)
endif ()

message (STATUS "${ColorBoldWhite}")
message (STATUS "* Checking for dependencies...")
message (STATUS "*   - Missing a dependency 'Package'?")
message (STATUS "*     Try cmake -DPackage_ROOT=path or set environment var Package_ROOT=path")
message (STATUS "*     For many dependencies, we supply src/build-scripts/build_Package.bash")
message (STATUS "*   - To exclude an optional dependency (even if found),")
message (STATUS "*     -DUSE_Package=OFF or set environment var USE_Package=OFF ")
message (STATUS "${ColorReset}")

set (OIIO_LOCAL_DEPS_PATH "${CMAKE_SOURCE_DIR}/ext/dist" CACHE STRING
     "Local area for dependencies added to CMAKE_PREFIX_PATH")
list (APPEND CMAKE_PREFIX_PATH ${OIIO_LOCAL_DEPS_PATH})

include (ExternalProject)

option (BUILD_MISSING_DEPS "Try to download and build any missing dependencies" OFF)

include (FindThreads)


###########################################################################
# Dependencies for required formats and features. These are so critical
# that we will not complete the build if they are not found.

checked_find_package (ZLIB REQUIRED)  # Needed by several packages

# Help set up this target for libtiff config file when using static libtiff
if (NOT TARGET CMath::CMath)
    find_library (MATH_LIBRARY m)
    if (NOT MATH_LIBRARY-NOTFOUND)
        add_library (CMath::CMath UNKNOWN IMPORTED)
        set_property (TARGET CMath::CMath
                      APPEND PROPERTY IMPORTED_LOCATION  ${MATH_LIBRARY})
    endif ()
endif ()

checked_find_package (TIFF REQUIRED
                      VERSION_MIN 3.9
                      RECOMMEND_MIN 4.0
                      RECOMMEND_MIN_REASON "to support >4GB files")

# IlmBase & OpenEXR
checked_find_package (Imath REQUIRED
                      VERSION_MIN 3.1
                      PRINT IMATH_INCLUDES Imath_VERSION)
checked_find_package (OpenEXR REQUIRED
                      VERSION_MIN 3.1
                      PRINT IMATH_INCLUDES OPENEXR_INCLUDES Imath_VERSION)
# Force Imath includes to be before everything else to ensure that we have
# the right Imath/OpenEXR version, not some older version in the system
# library. This shouldn't be necessary, except for the common case of people
# building against Imath/OpenEXR 3.x when there is still a system-level
# install version of 2.x.
include_directories(BEFORE ${IMATH_INCLUDES} ${OPENEXR_INCLUDES})
if (MSVC AND NOT LINKSTATIC)
    proj_add_compile_definitions (OPENEXR_DLL) # Is this needed for new versions?
endif ()
set (OIIO_USING_IMATH 3)
set (OPENIMAGEIO_IMATH_TARGETS
            $<TARGET_NAME_IF_EXISTS:Imath::Imath>
            $<TARGET_NAME_IF_EXISTS:Imath::Half> )
set (OPENIMAGEIO_OPENEXR_TARGETS
            $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR> )
set (OPENIMAGEIO_IMATH_DEPENDENCY_VISIBILITY "PUBLIC" CACHE STRING
     "Should we expose Imath library dependency as PUBLIC or PRIVATE")
set (OPENIMAGEIO_CONFIG_DO_NOT_FIND_IMATH OFF CACHE BOOL
     "Exclude find_dependency(Imath) from the exported OpenImageIOConfig.cmake")

# JPEG -- prefer JPEG-Turbo to regular libjpeg
checked_find_package (libjpeg-turbo
                      VERSION_MIN 2.1
                      DEFINITIONS -DUSE_JPEG_TURBO=1)
if (NOT TARGET libjpeg-turbo::jpeg) # Try to find the non-turbo version
    checked_find_package (JPEG REQUIRED)
endif ()

# JPEG XL
option (USE_JXL "Enable JPEG XL support" ON)
checked_find_package (JXL
                      VERSION_MIN 0.10.1
                      DEFINITIONS -DUSE_JXL=1)

# Pugixml setup.  Normally we just use the version bundled with oiio, but
# some linux distros are quite particular about having separate packages so we
# allow this to be overridden to use the distro-provided package if desired.
option (USE_EXTERNAL_PUGIXML "Use an externally built shared library version of the pugixml library" OFF)
if (USE_EXTERNAL_PUGIXML)
    checked_find_package (pugixml REQUIRED
                          VERSION_MIN 1.8
                          DEFINITIONS -DUSE_EXTERNAL_PUGIXML=1)
else ()
    message (STATUS "Using internal PugiXML")
endif()

# From pythonutils.cmake
find_python()
if (USE_PYTHON)
    checked_find_package (pybind11 REQUIRED VERSION_MIN 2.4.2)
endif ()


###########################################################################
# Dependencies for optional formats and features. If these are not found,
# we will continue building, but the related functionality will be disabled.

checked_find_package (PNG)

checked_find_package (BZip2)   # Used by ffmpeg and freetype
if (NOT BZIP2_FOUND)
    set (BZIP2_LIBRARIES "")  # TODO: why does it break without this?
endif ()

checked_find_package (Freetype
                   DEFINITIONS  -DUSE_FREETYPE=1 )

checked_find_package (OpenColorIO
                      DEFINITIONS  -DUSE_OCIO=1 -DUSE_OPENCOLORIO=1
                      # PREFER_CONFIG
                      )
if (OpenColorIO_FOUND)
    option (OIIO_DISABLE_BUILTIN_OCIO_CONFIGS
           "For deveoper debugging/testing ONLY! Disable OCIO 2.2 builtin configs." OFF)
    if (OIIO_DISABLE_BUILTIN_OCIO_CONFIGS OR "$ENV{OIIO_DISABLE_BUILTIN_OCIO_CONFIGS}")
        proj_add_compile_definitions(OIIO_DISABLE_BUILTIN_OCIO_CONFIGS)
    endif ()
else ()
    set (OpenColorIO_FOUND 0)
endif ()

checked_find_package (OpenCV 3.0
                   DEFINITIONS  -DUSE_OPENCV=1)

# Intel TBB
set (TBB_USE_DEBUG_BUILD OFF)
checked_find_package (TBB 2017
                      SETVARIABLES OIIO_TBB
                      PREFER_CONFIG)

# DCMTK is used to read DICOM images
checked_find_package (DCMTK CONFIG VERSION_MIN 3.6.1)

checked_find_package (FFmpeg VERSION_MIN 3.0)
checked_find_package (GIF
                      VERSION_MIN 4
                      RECOMMEND_MIN 5.0
                      RECOMMEND_MIN_REASON "for stability and thread safety")

# For HEIF/HEIC/AVIF formats
checked_find_package (Libheif VERSION_MIN 1.3
                      RECOMMEND_MIN 1.16
                      RECOMMEND_MIN_REASON "for orientation support")
if (APPLE AND LIBHEIF_VERSION VERSION_GREATER_EQUAL 1.10 AND LIBHEIF_VERSION VERSION_LESS 1.11)
    message (WARNING "Libheif 1.10 on Apple is known to be broken, disabling libheif support")
    set (Libheif_FOUND 0)
endif ()

checked_find_package (LibRaw
                      VERSION_MIN 0.20.0
                      PRINT LibRaw_r_LIBRARIES)

checked_find_package (OpenJPEG VERSION_MIN 2.0
                      RECOMMEND_MIN 2.2
                      RECOMMEND_MIN_REASON "for multithreading support")
# Note: Recent OpenJPEG versions have exported cmake configs, but we don't
# find them reliable at all, so we stick to our FindOpenJPEG.cmake module.

checked_find_package (OpenVDB
                      VERSION_MIN  9.0
                      DEPS         TBB
                      DEFINITIONS  -DUSE_OPENVDB=1)

checked_find_package (Ptex PREFER_CONFIG)
if (NOT Ptex_FOUND OR NOT Ptex_VERSION)
    # Fallback for inadequate Ptex exported configs. This will eventually
    # disappear when we can 100% trust Ptex's exports.
    unset (Ptex_FOUND)
    checked_find_package (Ptex)
endif ()

checked_find_package (WebP)
# Note: When WebP 1.1 (released late 2019) is our minimum, we can use their
# exported configs and remove our FindWebP.cmake module.

option (USE_R3DSDK "Enable R3DSDK (RED camera) support" OFF)
checked_find_package (R3DSDK NO_RECORD_NOTFOUND)  # RED camera

set (NUKE_VERSION "7.0" CACHE STRING "Nuke version to target")
checked_find_package (Nuke NO_RECORD_NOTFOUND)


# Qt -- used for iv
option (USE_QT "Use Qt if found" ON)
if (USE_QT)
    checked_find_package (OpenGL)   # used for iv
endif ()
if (USE_QT AND OPENGL_FOUND)
    checked_find_package (Qt6 COMPONENTS Core Gui Widgets OpenGLWidgets)
    if (NOT Qt6_FOUND)
        checked_find_package (Qt5 COMPONENTS Core Gui Widgets OpenGL)
    endif ()
    if (NOT Qt5_FOUND AND NOT Qt6_FOUND AND APPLE)
        message (STATUS "  If you think you installed qt with Homebrew and it still doesn't work,")
        message (STATUS "  try:   export PATH=/usr/local/opt/qt/bin:$PATH")
    endif ()
endif ()


###########################################################################
# Tessil/robin-map

option (BUILD_ROBINMAP_FORCE "Force local download/build of robin-map even if installed" OFF)
option (BUILD_MISSING_ROBINMAP "Local download/build of robin-map if not installed" ON)
set (BUILD_ROBINMAP_VERSION "v0.6.2" CACHE STRING "Preferred Tessil/robin-map version, of downloading/building our own")

macro (find_or_download_robin_map)
    # If we weren't told to force our own download/build of robin-map, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_ROBINMAP_FORCE)
        find_package (Robinmap QUIET)
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    # Download the headers from github
    if ((BUILD_MISSING_ROBINMAP AND NOT ROBINMAP_FOUND) OR BUILD_ROBINMAP_FORCE)
        message (STATUS "Downloading local Tessil/robin-map")
        set (ROBINMAP_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/robin-map")
        set (ROBINMAP_GIT_REPOSITORY "https://github.com/Tessil/robin-map")
        if (NOT IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/include/tsl)
            find_package (Git REQUIRED)
            execute_process(COMMAND             ${GIT_EXECUTABLE} clone    ${ROBINMAP_GIT_REPOSITORY} -n ${ROBINMAP_INSTALL_DIR})
            execute_process(COMMAND             ${GIT_EXECUTABLE} checkout ${BUILD_ROBINMAP_VERSION}
                            WORKING_DIRECTORY   ${ROBINMAP_INSTALL_DIR})
            if (IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/include/tsl)
                message (STATUS "DOWNLOADED Tessil/robin-map to ${ROBINMAP_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download Tessil/robin-map")
            endif ()
        endif ()
        set (ROBINMAP_INCLUDE_DIR "${ROBINMAP_INSTALL_DIR}/include")
    endif ()
    checked_find_package (Robinmap REQUIRED)
endmacro()

find_or_download_robin_map ()


###########################################################################
# fmtlib

option (BUILD_FMT_FORCE "Force local download/build of fmt even if installed" OFF)
option (BUILD_MISSING_FMT "Local download/build of fmt if not installed" ON)
option (INTERNALIZE_FMT "Copy fmt headers into <install>/include/OpenImageIO/detail/fmt" ON)
set (BUILD_FMT_VERSION "10.0.0" CACHE STRING "Preferred fmtlib/fmt version, when downloading/building our own")

macro (find_or_download_fmt)
    # If we weren't told to force our own download/build of fmt, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_FMT_FORCE)
        find_package (fmt QUIET)
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    if ((BUILD_MISSING_FMT AND NOT fmt_FOUND) OR BUILD_FMT_FORCE)
        message (STATUS "Downloading local fmtlib/fmt")
        set (FMT_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/fmt")
        set (FMT_GIT_REPOSITORY "https://github.com/fmtlib/fmt")
        if (NOT IS_DIRECTORY ${FMT_INSTALL_DIR}/include/fmt)
            find_package (Git REQUIRED)
            execute_process(COMMAND             ${GIT_EXECUTABLE} clone    ${FMT_GIT_REPOSITORY} -n ${FMT_INSTALL_DIR})
            execute_process(COMMAND             ${GIT_EXECUTABLE} checkout ${BUILD_FMT_VERSION}
                            WORKING_DIRECTORY   ${FMT_INSTALL_DIR})
            if (IS_DIRECTORY ${FMT_INSTALL_DIR}/include/fmt)
                message (STATUS "DOWNLOADED fmtlib/fmt to ${FMT_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download fmtlib/fmt")
            endif ()
        endif ()
        set (FMT_INCLUDE_DIR "${FMT_INSTALL_DIR}/include")
        set (OIIO_USING_FMT_LOCAL TRUE)
        if (EXISTS "${FMT_INCLUDE_DIR}/fmt/base.h")
            file (STRINGS "${FMT_INCLUDE_DIR}/fmt/base.h" TMP REGEX "^#define FMT_VERSION .*$")
        else ()
            file (STRINGS "${FMT_INCLUDE_DIR}/fmt/core.h" TMP REGEX "^#define FMT_VERSION .*$")
        endif ()
        string (REGEX MATCHALL "[0-9]+" FMT_VERSION_NUMERIC ${TMP})
        math(EXPR FMT_VERSION_PATCH "${FMT_VERSION_NUMERIC} % 100")
        math(EXPR FMT_VERSION_MINOR "(${FMT_VERSION_NUMERIC} / 100) % 100")
        math(EXPR FMT_VERSION_MAJOR "${FMT_VERSION_NUMERIC} / 10000")
        set (fmt_VERSION "${FMT_VERSION_MAJOR}.${FMT_VERSION_MINOR}.${FMT_VERSION_PATCH}")
        list (APPEND CFP_ALL_BUILD_DEPS_FOUND "${pkgname} ${${pkgname}_VERSION}")
    else ()
        get_target_property(FMT_INCLUDE_DIR fmt::fmt-header-only INTERFACE_INCLUDE_DIRECTORIES)
        set (OIIO_USING_FMT_LOCAL FALSE)
        checked_find_package (fmt REQUIRED
                              VERSION_MIN 7.0)
    endif ()
endmacro()

find_or_download_fmt()

if (fmt_VERSION VERSION_EQUAL 9.1.0
        AND GCC_VERSION VERSION_GREATER 0.0 AND NOT GCC_VERSION VERSION_GREATER 7.2)
    message (WARNING "${ColorRed}fmt 9.1 is known to not work with gcc <= 7.2${ColorReset}")
endif ()

list (SORT CFP_ALL_BUILD_DEPS_FOUND COMPARE STRING CASE INSENSITIVE)
message (STATUS "All build dependencies: ${CFP_ALL_BUILD_DEPS_FOUND}")
