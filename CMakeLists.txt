# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

cmake_minimum_required (VERSION 3.12)
project (OpenImageIO VERSION 2.1.20.0
         HOMEPAGE_URL "https://openimageio.org"
         LANGUAGES CXX C)
set (PROJ_NAME OIIO)    # short name, caps
string (TOLOWER ${PROJ_NAME} PROJ_NAME_LOWER)  # short name lower case
string (TOUPPER ${PROJ_NAME} PROJ_NAME_UPPER)  # short name upper case
set (PROJECT_VERSION_RELEASE_TYPE "")   # "dev", "betaX", "RCY", ""
set (${PROJECT_NAME}_VERSION_RELEASE_TYPE ${PROJECT_VERSION_RELEASE_TYPE})
set (PROJECT_AUTHORS "Contributors to the OpenImageIO project")

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

message (STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION}")
message (STATUS "CMake version is ${CMAKE_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

message (STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")
message (STATUS "CMake ${CMAKE_VERSION}")
message (STATUS "CMake system           = ${CMAKE_SYSTEM}")
message (STATUS "CMake system name      = ${CMAKE_SYSTEM_NAME}")
message (STATUS "Project source dir     = ${PROJECT_SOURCE_DIR}")
message (STATUS "Project build dir      = ${CMAKE_BINARY_DIR}")
message (STATUS "Project install prefix = ${CMAKE_INSTALL_PREFIX}")
message (STATUS "Configuration types    = ${CMAKE_CONFIGURATION_TYPES}")
message (STATUS "Build type             = ${CMAKE_BUILD_TYPE}")

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
option (${PROJ_NAME}_BUILD_TOOLS "Build the command-line tools" ON)
option (${PROJ_NAME}_BUILD_TESTS "Build the unit tests" ON)
set (SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
     CACHE STRING "Set the SO version in the SO name of the output library")
set (OIIO_LIBNAME_SUFFIX "" CACHE STRING
     "Optional name appended to ${PROJECT_NAME} libraries that are built")
option (BUILD_OIIOUTIL_ONLY "If ON, will build *only* libOpenImageIO_Util" OFF)
option (BUILD_DOCS "If ON, build documentation and man pages." ON)
option (INSTALL_DOCS "If ON, install documentation and man pages." ON)
option (INSTALL_FONTS "If ON, install default fonts" ON)
option (INSTALL_CMAKE_HELPER "If On, install FindOpenImageIO.cmake" ON)
option (EMBEDPLUGINS "Embed format plugins in libOpenImageIO" ON)
set (PLUGIN_SEARCH_PATH "" CACHE STRING "Default plugin search path")
file (TO_NATIVE_PATH "${PLUGIN_SEARCH_PATH}" PLUGIN_SEARCH_PATH_NATIVE)
set (CMAKE_DEBUG_POSTFIX "" CACHE STRING "Library naming postfix for Debug builds (e.g., '_debug')")

set(OIIO_TESTSUITE_IMAGEDIR "${PROJECT_SOURCE_DIR}/.." CACHE PATH
    "Location of oiio-images, openexr-images, libtiffpic, etc.." )

option (OIIO_THREAD_ALLOW_DCLP "OIIO threads may use DCLP for speed" ON)
if (NOT OIIO_THREAD_ALLOW_DCLP)
    add_definitions ("-DOIIO_THREAD_ALLOW_DCLP=0")
endif ()

set (TEX_BATCH_SIZE "" CACHE STRING "Force TextureSystem SIMD batch size (e.g. 16)")
if (TEX_BATCH_SIZE)
    add_definitions ("-DOIIO_TEXTURE_SIMD_BATCH_WIDTH=${TEX_BATCH_SIZE}")
endif ()

# Set the default namespace
set (${PROJ_NAME}_NAMESPACE ${PROJECT_NAME} CACHE STRING
     "Customized outer namespace base name (version will be added)")
option (${PROJ_NAME}_NAMESPACE_INCLUDE_PATCH "Should the inner namespace include the patch number" OFF)
set (PROJ_NAMESPACE "${${PROJ_NAME}_NAMESPACE}")
set (PROJ_NAMESPACE_V "${PROJ_NAMESPACE}_v${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}")
if (OIIO_NAMESPACE_INCLUDE_PATCH)
    set (PROJ_NAMESPACE_V "${PROJ_NAMESPACE_V}_${PROJECT_VERSION_PATCH}")
endif ()
message(STATUS "Setting Namespace to: ${PROJ_NAMESPACE_V}")


list (APPEND CMAKE_MODULE_PATH
      "${PROJECT_SOURCE_DIR}/src/cmake/modules"
      "${PROJECT_SOURCE_DIR}/src/cmake")

include (GNUInstallDirs)

include (colors)
include (check_is_enabled)

# All the C++ and compiler related options and adjustments live here
include (compiler)

include (oiio_macros)

# All the dependency finding and options are here
include (externalpackages)

# Utilities related to finding python and making python bindings
include (pythonutils)

# We want CTest for testing, so this must be added before all the calls to
# add_subdirectory, or their add_test commands will not register.
include (CTest)


include_directories (
    BEFORE
    "${CMAKE_SOURCE_DIR}/src/include"
    "${CMAKE_BINARY_DIR}/src/include"
    "${CMAKE_BINARY_DIR}/include"
  )

# Tell CMake to process the sub-directories
add_subdirectory (src/libutil)

find_or_download_robin_map ()


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

if (USE_PYTHON AND NOT BUILD_OIIOUTIL_ONLY)
    find_or_download_pybind11 ()
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
IF ( NOT MSVC )
   configure_file(src/build-scripts/OpenImageIO.pc.in "${CMAKE_BINARY_DIR}/OpenImageIO.pc" @ONLY)
   install (FILES "${CMAKE_BINARY_DIR}/OpenImageIO.pc"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
            COMPONENT developer)
ENDIF()

# Oddball: install the FindOpenImageIO.cmake file
install (FILES src/cmake/modules/FindOpenImageIO.cmake
         DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/Modules
         COMPONENT cmake)

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


#########################################################################
# Testing
#
# Just call oiio_add_tests(testname...) for each test.  Additional
# optional arguments include:
#     FOUNDVAR   specifies the name of a CMAKE variable; if not defined,
#                    the test will not be added for 'make test' (helpful
#                    for excluding tests for libraries not found).
#     IMAGEDIR   specifies a directory for test images, one level higher
#                    than where the oiio top level source lives -- a
#                    message will be printed if not found.
#     URL        URL where the test images can be found, will be
#                    incorporated into the error message if the test
#                    image directory is not found.
#     LABEL      If set to "broken", will designate the test as one
#                    that is known to be broken, so will only be run
#                    for "make testall", but not "make test".
#

# Make a build/platform/testsuite directory, and copy the master runtest.py
# there. The rest is up to the tests themselves.
file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/testsuite")
file (COPY "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/common"
      DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/testsuite")
add_custom_command (OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/runtest.py"
                        "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py"
                    MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/testsuite/runtest.py")
add_custom_target ( CopyFiles ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/testsuite/runtest.py" )

# Basic tests that apply even to continuous integration tests:

#   Tests that require oiio-images:
oiio_add_tests (gpsread
                oiiotool oiiotool-attribs  oiiotool-copy
                oiiotool-readerror oiiotool-xform
                oiiotool-fixnan ## maketx & oiiotool-maketx depend on this
                maketx oiiotool-maketx
                misnamed-file
                missingcolor
                dpx ico iff png psd rla sgi zfile
                texture-interp-bicubic
                texture-blurtube
                texture-crop texture-cropover
                texture-derivs texture-fill texture-filtersize
                texture-flipt texture-gettexels texture-gray
                texture-mip-nomip texture-mip-trilinear
                texture-overscan texture-pointsample
                texture-uint8
                texture-width0blur
                texture-fat texture-skinny texture-wrapfill
                texture-missing texture-res texture-maxres
                texture-udim texture-udim2
                IMAGEDIR oiio-images
               )

#   Tests that require openexr-images:
oiio_add_tests (oiiotool-deep
                IMAGEDIR openexr-images
               )

#   Remaining freestanding tests:
oiio_add_tests (nonwhole-tiles
                oiiotool-composite
                oiiotool-pattern
                oiiotool-subimage oiiotool-text
                diff
                dither dup-channels
                jpeg-corrupt
                null psd-colormodes
                rational
               )

# Travis + old libjpeg seems to not catch an error in this test, skip it
# for that case.
if (JPEG_TURBO_FOUND OR NOT DEFINED ENV{TRAVIS})
    oiio_add_tests (jpeg-corrupt-header)
endif()

# Add tests that require the Python bindings if we built the Python
# bindings. This is mostly the test that are specifically about testing
# the Python bindings themselves, but also a handful of tests that are
# mainly about other things but happen to use Python in order to perform
# thee test.
# We also exclude these tests if this is a sanitizer build on Linux,
# because the Python interpreter itself won't be linked with the right asan
# libraries to run correctly.
if (USE_PYTHON AND NOT BUILD_OIIOUTIL_ONLY AND NOT SANITIZE_ON_LINUX)
    oiio_add_tests (
            python-typedesc python-paramlist
            python-imagespec python-roi python-deep python-colorconfig
            python-imageinput python-imageoutput
            python-imagebuf python-imagebufalgo
            IMAGEDIR oiio-images
            )
endif ()

# Add tests that require OCIO
if (OPENCOLORIO_FOUND)
    oiio_add_tests (oiiotool-color)
endif ()

# Advanced tests that are done by hand and for releases:
# FIXME -- at some point, try to fix these or provide new ref images
if (NOT DEFINED ENV{TRAVIS})
    oiio_add_tests (
                    texture-half texture-uint16
                    texture-interp-bilinear
                    texture-interp-closest
                    texture-mip-onelevel
                   )
endif ()
if (NOT DEFINED ENV{TRAVIS} AND NOT DEFINED ENV{CIRCLECI} AND NOT DEFINED ENV{GITHUB_ACTIONS})
    oiio_add_tests (texture-icwrite)
endif ()

# List testsuites which need special external reference images from the web
# here:
oiio_add_tests (bmp
    IMAGEDIR bmpsuite
    URL http://entropymine.com/jason/bmpsuite/bmpsuite.zip)

oiio_add_tests (tiff-suite tiff-depths tiff-misc
    IMAGEDIR libtiffpic
    URL http://www.simplesystems.org/libtiff/images.html)

oiio_add_tests (openexr-suite openexr-multires openexr-chroma openexr-v2 perchannel
    IMAGEDIR openexr-images
    URL http://www.openexr.com/downloads.html)

oiio_add_tests (gif
    FOUNDVAR GIF_FOUND
    IMAGEDIR oiio-images
    URL "Recent checkout of oiio-images")

oiio_add_tests (jpeg2000
    FOUNDVAR OPENJPEG_FOUND
    IMAGEDIR j2kp4files_v1_5
    URL http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803)

oiio_add_tests (pnm
    IMAGEDIR oiio-images/pnm
    URL "Recent checkout of oiio-images")

oiio_add_tests (raw
    FOUNDVAR LIBRAW_FOUND
    IMAGEDIR oiio-images/raw
    URL "Recent checkout of oiio-images")

oiio_add_tests (targa-tgautils
    IMAGEDIR TGAUTILS
    URL http://tgautils.inequation.org/)

oiio_add_tests (fits
    IMAGEDIR fits-images
    URL http://www.cv.nrao.edu/fits/data/tests/)

oiio_add_tests (heif
    FOUNDVAR LIBHEIF_FOUND
    URL https://github.com/nokiatech/heif/tree/gh-pages/content)

oiio_add_tests (webp
    FOUNDVAR WEBP_FOUND
    IMAGEDIR oiio-images/webp
    URL "Recent checkout of oiio-images")

oiio_add_tests (ptex
    FOUNDVAR PTEX_FOUND)

oiio_add_tests (texture-field3d field3d
    FOUNDVAR FIELD3D_FOUND)


oiio_add_tests (openvdb
    FOUNDVAR OPENVDB_FOUND)

if (SPI_TESTS)
  oiio_add_tests (oiiotool-spi
    FOUNDVAR SPI_TESTS
    IMAGEDIR spi-oiio-tests
    URL "noplace -- it's SPI specific tests")
endif ()



#########################################################################
# Packaging
set (CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
# "Vendor" is only used in copyright notices, so we use the same thing that
# the rest of the copyright notices say.
set (CPACK_PACKAGE_VENDOR ${PROJECT_AUTHORS})
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenImageIO is an open source library for reading and writing image file formats, a nice format-agnostic image viewer, and other image-related classes and utilities.")
set (CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/src/doc/Description.txt")
set (CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
#SET (CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_SOURCE_DIR}")
file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cpack")
file (COPY "${PROJECT_SOURCE_DIR}/LICENSE.md" DESTINATION "${CMAKE_BINARY_DIR}/cpack")
file (RENAME "${CMAKE_BINARY_DIR}/cpack/LICENSE.md" "${CMAKE_BINARY_DIR}/cpack/License.txt")
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/cpack/License.txt")
file (COPY "${PROJECT_SOURCE_DIR}/README.md" DESTINATION "${CMAKE_BINARY_DIR}/cpack")
set (CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/cpack/README.md")
set (CPACK_RESOURCE_FILE_WELCOME "${PROJECT_SOURCE_DIR}/src/doc/Welcome.txt")
#SET (CPACK_STRIP_FILES Do we need this?)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (CPACK_GENERATOR "TGZ;STGZ;RPM;DEB")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()
if (APPLE)
    set (CPACK_GENERATOR "TGZ;STGZ;PackageMaker")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()
if (WIN32)
    set (CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_EXECUTABLES "iv" "iv - Image Viewer")
#    set(CPACK_CREATE_DESCTOP_LINKS "iv" "iv - Image Viewer")
    set(CPACK_NSIS_MODIFY_PATH ON)
    include (InstallRequiredSystemLibraries)
endif ()
set (CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-source)
#set (CPACK_SOURCE_STRIP_FILES Do we need this?)
set (CPACK_SOURCE_IGNORE_FILES ".*~")
set (CPACK_COMPONENT_UNSPECIFIED_HIDDEN TRUE)
set (CPACK_COMPONENT_UNSPECIFIED_REQUIRED TRUE)
set (CPACK_COMPONENTS_ALL user developer documentation Unspecified)
set (CPACK_COMPONENT_USER_DISPLAY_NAME "Applications")
set (CPACK_COMPONENT_DEVELOPER_DISPLAY_NAME "Developer files")
set (CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set (CPACK_COMPONENT_USER_DESCRIPTION
     "Applications: oiotool, iv, iinfo, iconvert, idiff, igrep, maketx and libraries")
set (CPACK_COMPONENT_DEVELOPER_DESCRIPTION "Include files")
set (CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "OpenImageIO documentation")
set (CPACK_COMPONENT_DEVELOPER_DEPENDS user)
include (CPack)
