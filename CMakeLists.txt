# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.15)

set (OpenImageIO_VERSION "2.5.12.0")
set (OpenImageIO_VERSION_OVERRIDE "" CACHE STRING
     "Version override (use with caution)!")
mark_as_advanced (OpenImageIO_VERSION_OVERRIDE)
if (OpenImageIO_VERSION_OVERRIDE)
    set (OpenImageIO_VERSION ${OpenImageIO_VERSION_OVERRIDE})
endif ()

project (OpenImageIO VERSION ${OpenImageIO_VERSION}
         HOMEPAGE_URL "https://openimageio.org"
         LANGUAGES CXX C)

set (PROJ_NAME OIIO)    # short name, caps
string (TOLOWER ${PROJ_NAME} PROJ_NAME_LOWER)  # short name lower case
string (TOUPPER ${PROJ_NAME} PROJ_NAME_UPPER)  # short name upper case
set (PROJECT_VERSION_RELEASE_TYPE "" CACHE STRING
    "Build type, for example: dev, beta2, RC1 (empty string for normal release)")
set (${PROJECT_NAME}_VERSION_RELEASE_TYPE ${PROJECT_VERSION_RELEASE_TYPE})
set (PROJECT_AUTHORS "Contributors to the OpenImageIO project")
option (${PROJECT_NAME}_SUPPORTED_RELEASE
        "Set ON for supported release branch, OFF for master" ON)
if (${PROJECT_NAME}_SUPPORTED_RELEASE)
    set (${PROJECT_NAME}_DEV_RELEASE OFF)
else ()
    set (${PROJECT_NAME}_DEV_RELEASE ON)
endif ()

# Set PROJECT_IS_TOP_LEVEL to ON if if this is the top level project (not
# if this is included as a subproject of something else). Note that this is
# handled automatically for CMake >= 3.21.
if (CMAKE_VERSION VERSION_LESS 3.21)
    if ("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
        set (PROJECT_IS_TOP_LEVEL ON)
    endif ()
endif ()

# If the user wants to use Conan to build dependencies, they will have done
# this prior to the cmake config:
#   cd <build area>
#   conan install <source area>
# and that will leave a conanbuildinfo.cmake in the build area for us.
if (EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include (${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    message (STATUS "Using Conan for dependencies")
    conan_basic_setup()
endif()

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

# If the user hasn't configured cmake with an explicit
# -DCMAKE_INSTALL_PREFIX=..., then set it to safely install into ./dist, to
# help prevent the user from accidentally writing over /usr/local or whatever.
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT
      AND PROJECT_IS_TOP_LEVEL)
    set (CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/dist" CACHE PATH
         "Installation location" FORCE)
endif()

message (STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")
message (STATUS "CMake ${CMAKE_VERSION}")
message (STATUS "CMake system           = ${CMAKE_SYSTEM}")
message (STATUS "CMake system name      = ${CMAKE_SYSTEM_NAME}")
message (STATUS "Project source dir     = ${PROJECT_SOURCE_DIR}")
message (STATUS "Project build dir      = ${CMAKE_BINARY_DIR}")
message (STATUS "Project install prefix = ${CMAKE_INSTALL_PREFIX}")
message (STATUS "Configuration types    = ${CMAKE_CONFIGURATION_TYPES}")
message (STATUS "Build type             = ${CMAKE_BUILD_TYPE}")
message (STATUS "Supported release      = ${${PROJECT_NAME}_SUPPORTED_RELEASE}")

# Make the build area layout look a bit more like the final dist layout
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if ("${PROJECT_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message (FATAL_ERROR "Not allowed to run in-source build!")
endif ()

option (CMAKE_USE_FOLDERS "Use the FOLDER target property to organize targets into folders." ON)
mark_as_advanced (CMAKE_USE_FOLDERS)
if (CMAKE_USE_FOLDERS)
    set_property (GLOBAL PROPERTY USE_FOLDERS ON)
endif ()


option (VERBOSE "Print lots of messages while compiling" OFF)
if (VERBOSE)
    set (CMAKE_MESSAGE_LOG_LEVEL "VERBOSE" CACHE STRING "CMake log level to display")
else ()
    set (CMAKE_MESSAGE_LOG_LEVEL "STATUS" CACHE STRING "CMake log level to display")
endif ()
option (${PROJ_NAME}_BUILD_TOOLS "Build the command-line tools" ON)
option (${PROJ_NAME}_BUILD_TESTS "Build the unit tests" ON)
set (OIIO_LIBNAME_SUFFIX "" CACHE STRING
     "Optional name appended to ${PROJECT_NAME} libraries that are built")
option (BUILD_OIIOUTIL_ONLY "If ON, will build *only* libOpenImageIO_Util" OFF)
option (BUILD_DOCS "If ON, build documentation and man pages." ON)
option (INSTALL_DOCS "If ON, install documentation and man pages." ON)
option (INSTALL_FONTS "If ON, install default fonts" ON)
option (EMBEDPLUGINS "Embed format plugins in libOpenImageIO" ON)
set (PLUGIN_SEARCH_PATH "" CACHE STRING "Default plugin search path")
file (TO_NATIVE_PATH "${PLUGIN_SEARCH_PATH}" PLUGIN_SEARCH_PATH_NATIVE)
set (CMAKE_DEBUG_POSTFIX "_d" CACHE STRING "Library naming postfix for Debug builds")

if (CMAKE_UNITY_BUILD_BATCH_SIZE)
    set (UNITY_SMALL_BATCH_SIZE "${CMAKE_UNITY_BUILD_BATCH_SIZE}" CACHE STRING "Unity batch mode size for expensive files")
else ()
    set (UNITY_SMALL_BATCH_SIZE 8 CACHE STRING "Unity batch mode size for expensive files")
endif ()
message(STATUS "CMAKE_UNITY_BUILD_MODE = ${CMAKE_UNITY_BUILD_MODE}")
message(STATUS "CMAKE_UNITY_BUILD_BATCH_SIZE = ${CMAKE_UNITY_BUILD_BATCH_SIZE}")

option (OIIO_THREAD_ALLOW_DCLP "OIIO threads may use DCLP for speed" ON)
if (NOT OIIO_THREAD_ALLOW_DCLP)
    add_definitions ("-DOIIO_THREAD_ALLOW_DCLP=0")
endif ()

set (TEX_BATCH_SIZE "" CACHE STRING "Force TextureSystem SIMD batch size (e.g. 16)")
if (TEX_BATCH_SIZE)
    add_definitions ("-DOIIO_TEXTURE_SIMD_BATCH_WIDTH=${TEX_BATCH_SIZE}")
endif ()
option (OIIO_TEX_IMPLEMENT_VARYINGREF "Implement the deprecated batch texture functions taking VaryingRef params" ON)
if (NOT OIIO_TEX_IMPLEMENT_VARYINGREF)
    add_definitions (-DOIIO_TEX_NO_IMPLEMENT_VARYINGREF=1)
endif ()

# Set the default namespace
set (${PROJ_NAME}_NAMESPACE ${PROJECT_NAME} CACHE STRING
     "Customized outer namespace base name (version will be added)")
option (${PROJ_NAME}_NAMESPACE_INCLUDE_PATCH
        "Should the inner namespace include the patch number" ${${PROJECT_NAME}_DEV_RELEASE})
set (PROJ_NAMESPACE "${${PROJ_NAME}_NAMESPACE}")
set (PROJ_NAMESPACE_V "${PROJ_NAMESPACE}_v${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}")
if (${PROJ_NAME}_NAMESPACE_INCLUDE_PATCH)
    set (PROJ_NAMESPACE_V "${PROJ_NAMESPACE_V}_${PROJECT_VERSION_PATCH}")
endif ()
message(STATUS "Setting Namespace to: ${PROJ_NAMESPACE_V}")

set (OIIO_SITE "" CACHE STRING "Site name for customization")
if (OIIO_SITE)
    add_definitions (-DOIIO_SITE_${OIIO_SITE})
endif ()

# Define OIIO_INTERNAL symbol only when building OIIO itself, will not be
# defined for downstream projects using OIIO.
add_definitions (-DOIIO_INTERNAL=1)

list (APPEND CMAKE_MODULE_PATH
      "${PROJECT_SOURCE_DIR}/src/cmake/modules"
      "${PROJECT_SOURCE_DIR}/src/cmake")

include (GNUInstallDirs)

# Utilities
include (colors)
include (check_is_enabled)
include (checked_find_package)
include (fancy_add_executable)
include (add_oiio_plugin)

# All the C++ and compiler related options and adjustments
include (compiler)

# Utilities and options related to finding python and making python bindings
include (pythonutils)

# Dependency finding utilities and all dependency-related options
include (externalpackages)

# Include all our testing apparatus and utils, but not if it's a subproject
if (PROJECT_IS_TOP_LEVEL)
    include (testing)
else ()
    macro (oiio_add_tests)
    endmacro ()
    macro (oiio_add_all_tests)
    endmacro ()
endif ()


include_directories (
    BEFORE
    "${CMAKE_SOURCE_DIR}/src/include"
    "${CMAKE_BINARY_DIR}/src/include"
    "${CMAKE_BINARY_DIR}/include"
    "${CMAKE_BINARY_DIR}/include/OpenImageIO"
  )

# Tell CMake to process the sub-directories
add_subdirectory (src/libutil)



# Add IO plugin directories -- if we are embedding plugins, we need to visit
# these directories BEFORE the OpenImageIO target is established (in
# src/libOpenImageIO). For each plugin, we append to the lists of source
# files, format libs, include directories, and definitions, all of which
# will be used by src/libOpenImageIO.
set (libOpenImageIO_srcs "")
set (format_plugin_libs "")
set (format_plugin_include_dirs "")
set (format_plugin_definitions "")
file (GLOB all_format_plugin_dirs src/*.imageio)
if ("${OIIO_SITE}" STREQUAL "SPI")
    # SPI only -- because of a workaround for a very weird linkage issue
    # specific to our system, we need to be sure libtiff is referenced first.
    file (GLOB tiff_format_plugin_dir src/tiff.imageio)
    list (REMOVE_ITEM all_format_plugin_dirs ${tiff_format_plugin_dir})
    list (INSERT all_format_plugin_dirs 0 ${tiff_format_plugin_dir})
endif ()
if (EMBEDPLUGINS AND NOT BUILD_OIIOUTIL_ONLY)
    foreach (plugin_dir ${all_format_plugin_dirs})
        add_subdirectory (${plugin_dir})
    endforeach ()
endif ()

if (NOT BUILD_OIIOUTIL_ONLY)
    add_subdirectory (src/libOpenImageIO)
endif ()

if (OIIO_BUILD_TOOLS AND NOT BUILD_OIIOUTIL_ONLY)
    add_subdirectory (src/iconvert)
    add_subdirectory (src/idiff)
    add_subdirectory (src/igrep)
    add_subdirectory (src/iinfo)
    add_subdirectory (src/maketx)
    add_subdirectory (src/oiiotool)
    add_subdirectory (src/testtex)
    add_subdirectory (src/iv)
endif ()

# Add IO plugin directories -- if we are not embedding plugins, we need to
# do it AFTER the OpenImageIO target is established (in src/libOpenImageIO),
# since each plugin needs libOpenImageIO to be a dependency.
if (NOT EMBEDPLUGINS AND NOT BUILD_OIIOUTIL_ONLY)
    foreach (plugin_dir ${all_format_plugin_dirs})
        add_subdirectory (${plugin_dir})
    endforeach ()
endif ()

if (USE_PYTHON AND Python_Development_FOUND AND NOT BUILD_OIIOUTIL_ONLY)
    add_subdirectory (src/python)
endif ()

add_subdirectory (src/include)
if (BUILD_DOCS)
    add_subdirectory (src/doc)
endif ()
add_subdirectory (src/fonts)

if (NUKE_FOUND)
    add_subdirectory (src/nuke/txReader)
    add_subdirectory (src/nuke/txWriter)
endif ()

# Last minute site-specific instructions, if they exist
if (OIIO_SITE AND EXISTS "${PROJECT_SOURCE_DIR}/site/${OIIO_SITE}/cmake/sitecustom.cmake")
    include ("${PROJECT_SOURCE_DIR}/site/${OIIO_SITE}/cmake/sitecustom.cmake")
endif ()

# install pkgconfig file
if (NOT MSVC)
   configure_file(src/build-scripts/OpenImageIO.pc.in "${CMAKE_BINARY_DIR}/OpenImageIO.pc" @ONLY)
   install (FILES "${CMAKE_BINARY_DIR}/OpenImageIO.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
            COMPONENT developer)
endif()

# Export the configuration files. There are also library-specific config
# exports in the CMakeLists.txt of libOpenImageIO.
include (CMakePackageConfigHelpers)

# the file containing the exported targets
set (OIIO_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets.cmake")
# the version file
set (OIIO_VERSION_CONFIG "${CMAKE_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
# the config file that is actually looked for by find_package
set (OIIO_PROJECT_CONFIG "${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake")
# where all these files will be installed
set (OIIO_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# first generate the version file in the binary dir
write_basic_package_version_file (
        ${OIIO_VERSION_CONFIG}
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

# generate the Targets file in the binary dir using the targets collected in
# OIIO_EXPORTED_TARGETS each target is added to OIIO_EXPORTED_TARGETS
# through the macro install_target().
export (EXPORT OIIO_EXPORTED_TARGETS FILE "${CMAKE_BINARY_DIR}/${OIIO_TARGETS_EXPORT_NAME}")

# generate the config file from the template in the binary dir
configure_package_config_file ("${PROJECT_SOURCE_DIR}/src/cmake/Config.cmake.in"
        "${OIIO_PROJECT_CONFIG}"
        INSTALL_DESTINATION "${OIIO_CONFIG_INSTALL_DIR}")

# generate the config file from the template in the binary dir
install (FILES "${OIIO_PROJECT_CONFIG}" "${OIIO_VERSION_CONFIG}"
        DESTINATION "${OIIO_CONFIG_INSTALL_DIR}")

# install targets files
install (EXPORT OIIO_EXPORTED_TARGETS
        DESTINATION ${OIIO_CONFIG_INSTALL_DIR}
        FILE ${OIIO_TARGETS_EXPORT_NAME}
        NAMESPACE ${PROJECT_NAME}::)


if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING AND ${PROJ_NAME}_BUILD_TESTS)
    oiio_setup_test_data()
    oiio_add_all_tests()
endif ()

if (PROJECT_IS_TOP_LEVEL)
    include (packaging)
endif ()
