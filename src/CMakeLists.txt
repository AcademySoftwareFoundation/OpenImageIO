project (OpenImageIO)
set (OpenImageIO_VERSION_MAJOR 0)
set (OpenImageIO_VERSION_MINOR 6)
set (OpenImageIO_VERSION_PATCH 0)

cmake_minimum_required (VERSION 2.6)
set (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)
message (STATUS "Project source dir = ${PROJECT_SOURCE_DIR}")
message (STATUS "Project build dir = ${CMAKE_BINARY_DIR}")

if (${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message (FATAL_ERROR "Not allowed to run in-source build!")
endif ()


if (NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE "Release") 
endif ()

set (EMBEDPLUGINS OFF CACHE BOOL "Embed format plugins in libOpenImageIO")
set (USE_OPENGL ON CACHE BOOL "Include OpenGL support")
set (USE_QT ON CACHE BOOL "Include Qt support")
set (FORCE_OPENGL_1 OFF CACHE BOOL "Force iv to use OpenGL's fixed pipeline")
set (USE_TBB ON CACHE BOOL "Use TBB if needed")

set(CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/cmake/modules"
    "${PROJECT_SOURCE_DIR}/cmake")

include (util_macros)
include (oiio_macros)
include (platform)
include (externalpackages)


include_directories ("include")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions ("-DDEBUG=1")
endif ()

set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)



###########################################################################
if (MSVC)
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS)
endif (MSVC)


# Tell CMake to process the sub-directories
add_subdirectory (libOpenImageIO)

add_subdirectory (iconvert)
add_subdirectory (idiff)
add_subdirectory (igrep)
add_subdirectory (iinfo)
add_subdirectory (maketx)
add_subdirectory (iv)

# Add IO plugin directories
if (NOT EMBEDPLUGINS)
    add_subdirectory (bmp.imageio)
    add_subdirectory (hdr.imageio)
    add_subdirectory (ico.imageio)
    add_subdirectory (jpeg.imageio)
    add_subdirectory (openexr.imageio)
    add_subdirectory (png.imageio)
    add_subdirectory (targa.imageio)
    add_subdirectory (tiff.imageio)
    add_subdirectory (zfile.imageio)
endif ()

add_subdirectory (include)
add_subdirectory (doc)


#########################################################################
# Testing
include (CTest)
add_subdirectory (testtex)
add_subdirectory (test_libOpenImageIO)
add_test (libOpenImageIO ${CMAKE_BINARY_DIR}/test_libOpenImageIO/test_libOpenImageIO)
# Ugh, I can't seem to ADD_SUBDIRECTORY the testsuite dir if it's not a true
# subdirectory of source, so we fake it by creating a link.  FIXME!
# create_symlink doesn't work on Windows. There should be another solution FIXME!
if (NOT WIN32)
    exec_program ("cmake -E create_symlink ${PROJECT_SOURCE_DIR}/../testsuite ${PROJECT_SOURCE_DIR}/testsuite")
    add_subdirectory (testsuite)
endif ()


#########################################################################
# Packaging
set (CPACK_PACKAGE_VERSION_MAJOR ${OpenImageIO_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${OpenImageIO_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${OpenImageIO_VERSION_PATCH})
# "Vendor" is only used in copyright notices, so we use the same thing that
# the rest of the copyright notices say.
set (CPACK_PACKAGE_VENDOR "Larry Gritz et al.")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenImageIO is an open source library for reading and writing image file formats, a nice format-agnostic image viewer, and other image-related classes and utilities.")
set (CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/doc/Description.txt")
set (CPACK_PACKAGE_FILE_NAME OpenImageIO-${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}-${platform})
#SET (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_SOURCE_DIR}/..)
exec_program ("cmake -E copy ${PROJECT_SOURCE_DIR}/../LICENSE ${CMAKE_BINARY_DIR}/License.txt")
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/License.txt")
exec_program ("cmake -E copy ${PROJECT_SOURCE_DIR}/../README ${CMAKE_BINARY_DIR}/Readme.txt")
set (CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/Readme.txt")
set (CPACK_RESOURCE_FILE_WELCOME "${PROJECT_SOURCE_DIR}/doc/Welcome.txt")
#SET (CPACK_PACKAGE_EXECUTABLES I'm not sure what this is for)
#SET (CPACK_STRIP_FILES Do we need this?)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (CPACK_GENERATOR "TGZ;STGZ;RPM;DEB")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()
if (APPLE)
    set (CPACK_GENERATOR "TGZ;STGZ;PackageMaker")
    set (CPACK_SOURCE_GENERATOR "TGZ")
endif ()
set (CPACK_SOURCE_PACKAGE_FILE_NAME OpenImageIO-${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}-source)
#set (CPACK_SOURCE_STRIP_FILES Do we need this?)
set (CPACK_SOURCE_IGNORE_FILES ".*~")
include (CPack)

# TODO: equivalents of the old:
#  * make doxygen
#  * BOOST_DYNAMIC

# Do TIFF, JPEG, PNG actually look in external?
