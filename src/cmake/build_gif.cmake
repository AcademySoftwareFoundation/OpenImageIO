# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

###########################################################################
# Build GIF library locally
#
# This script downloads and builds the GIF library from source since GIF
# doesn't provide a CMake-based build system like other dependencies.
###########################################################################

# Set version and source information
set_cache(GIF_BUILD_VERSION 5.2.1 "GIFLIB version for local builds")
set(GIF_URL "https://downloads.sourceforge.net/project/giflib/giflib-${GIF_BUILD_VERSION}.tar.gz")
set(GIF_URL_HASH SHA256=31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd)

# Configuration options
set_cache(GIF_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
          DOC "Should execute a local GIF build, if necessary, build shared libraries" ADVANCED)

string(MAKE_C_IDENTIFIER ${GIF_BUILD_VERSION} GIF_VERSION_IDENT)

# Set up  variables 
set(GIF_LOCAL_SOURCE_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/GIF")
set(GIF_LOCAL_BUILD_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/GIF-build")
set(GIF_LOCAL_INSTALL_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/dist")

# Ensure necessary directories exist
file(MAKE_DIRECTORY ${GIF_LOCAL_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${GIF_LOCAL_INSTALL_DIR}/lib)

# Download and build using ExternalProject
# GIF uses a simple Makefile-based build system, so we skip the configure step
include(ExternalProject)
ExternalProject_Add(GIF_ext
    # Source location and verification
    URL             ${GIF_URL}
    URL_HASH        ${GIF_URL_HASH}
    DOWNLOAD_DIR    "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    
    # Build directories
    PREFIX          "${GIF_LOCAL_BUILD_DIR}"
    SOURCE_DIR      "${GIF_LOCAL_SOURCE_DIR}"
    
    # Build in source since GIF doesn't support out-of-source builds
    BUILD_IN_SOURCE 1
    
    # Build steps
    CONFIGURE_COMMAND ""  # Skip configure step (no configure script)
    BUILD_COMMAND   make -j${CMAKE_BUILD_PARALLEL_LEVEL} PREFIX=${GIF_LOCAL_INSTALL_DIR}
    INSTALL_COMMAND make PREFIX=${GIF_LOCAL_INSTALL_DIR} install
    
    # Apply a patch to fix the Makefile - with hash verification
    PATCH_COMMAND   ${CMAKE_COMMAND} -E echo "Downloading patch file..." &&
                curl -L https://sourceforge.net/p/giflib/bugs/_discuss/thread/4e811ad29b/c323/attachment/Makefile.patch -o Makefile.patch &&
                ${CMAKE_COMMAND} -E echo "Computing patch file hash..." &&
                ${CMAKE_COMMAND} -E md5sum Makefile.patch > patch_hash.txt &&
                ${CMAKE_COMMAND} -E compare_files patch_hash.txt ${PROJECT_SOURCE_DIR}/cmake/patches/giflib-5.2.1-expected-hash.txt &&
                ${CMAKE_COMMAND} -E echo "Patch file verified, applying..." &&
                patch -p0 < Makefile.patch
                    
    # Enable logging
    LOG_DOWNLOAD ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

# Add custom commands to ensure header file and library are properly installed
# This is needed because the GIF Makefile installation might be unreliable
add_custom_command(
    TARGET GIF_ext
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GIF_LOCAL_SOURCE_DIR}/gif_lib.h
        ${GIF_LOCAL_INSTALL_DIR}/include/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GIF_LOCAL_SOURCE_DIR}/libgif.a
        ${GIF_LOCAL_INSTALL_DIR}/lib/
)

#############################################################################
# Create a custom FindGIF.cmake module
# 
# The standard FindGIF.cmake module may have trouble locating the locally 
# built library, so we provide our own module that points directly to our
# build outputs.
#############################################################################

file(MAKE_DIRECTORY "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/cmake/modules")
file(WRITE "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/cmake/modules/FindGIF.cmake"
"# Custom FindGIF module for locally-built GIF
set(GIF_FOUND TRUE)
set(GIF_VERSION ${GIF_BUILD_VERSION})
set(GIF_INCLUDE_DIR \"${GIF_LOCAL_INSTALL_DIR}/include\")
set(GIF_LIBRARY \"${GIF_LOCAL_INSTALL_DIR}/lib/libgif.a\")
set(GIF_LIBRARIES \${GIF_LIBRARY})
set(GIF_INCLUDE_DIRS \${GIF_INCLUDE_DIR})

# Create imported target for use with modern CMake
if(NOT TARGET GIF::GIF)
  add_library(GIF::GIF UNKNOWN IMPORTED)
  set_target_properties(GIF::GIF PROPERTIES
    IMPORTED_LOCATION \"\${GIF_LIBRARY}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${GIF_INCLUDE_DIR}\"
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GIF
  REQUIRED_VARS GIF_LIBRARY GIF_INCLUDE_DIR
  VERSION_VAR GIF_VERSION
)
")

# Add our modules directory to the CMAKE_MODULE_PATH
list(APPEND CMAKE_MODULE_PATH "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/cmake/modules")

# Create the GIF::GIF imported target in the current scope
if(NOT TARGET GIF::GIF)
    add_library(GIF::GIF UNKNOWN IMPORTED GLOBAL)
    set_target_properties(GIF::GIF PROPERTIES
        IMPORTED_LOCATION "${GIF_LOCAL_INSTALL_DIR}/lib/libgif.a"
        INTERFACE_INCLUDE_DIRECTORIES "${GIF_LOCAL_INSTALL_DIR}/include"
    )
    add_dependencies(GIF::GIF GIF_ext)
endif()

# Set up variables for the find_package system
set(GIF_ROOT ${GIF_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set(GIF_REFIND TRUE)
set(GIF_REFIND_VERSION ${GIF_BUILD_VERSION})
set(GIF_REFIND_ARGS MODULE REQUIRED)

# Handle shared library installation if requested
if(GIF_BUILD_SHARED_LIBS)
    install_local_dependency_libs(GIF gif)
endif()