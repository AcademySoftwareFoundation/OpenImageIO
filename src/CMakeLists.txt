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
    set (CMAKE_BUILD_TYPE "Release") 
endif ()

set (EMBEDPLUGINS OFF CACHE BOOL "Embed format plugins in libOpenImageIO")
set (USE_OPENGL ON CACHE BOOL "Include OpenGL support")
set (USE_QT ON CACHE BOOL "Include Qt support")
set (FORCE_OPENGL_1 OFF CACHE BOOL "Force iv to use OpenGL's fixed pipeline")
set (USE_TBB ON CACHE BOOL "Use TBB if needed")

set (CMAKE_MODULE_PATH
     "${PROJECT_SOURCE_DIR}/cmake/modules"
     "${PROJECT_SOURCE_DIR}/cmake")

include (util_macros)
include (oiio_macros)
include (platform)
include (externalpackages)


# We want CTest for testing
include (CTest)

include_directories ("include")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions ("-DDEBUG=1")
endif ()

if (OPENIMAGEIO_NAMESPACE)
    add_definitions ("-DOPENIMAGEIO_NAMESPACE=${OPENIMAGEIO_NAMESPACE}")
endif ()

set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)



###########################################################################
if (MSVC)
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS)
    add_definitions (-DBOOST_ALL_DYN_LINK)
    add_definitions (-DOPENEXR_DLL)
endif (MSVC)


# Tell CMake to process the sub-directories
add_subdirectory (libOpenImageIO)

add_subdirectory (iconvert)
add_subdirectory (idiff)
add_subdirectory (igrep)
add_subdirectory (iinfo)
add_subdirectory (iprocess)
add_subdirectory (maketx)
add_subdirectory (testtex)
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

macro ( TESTSUITE )
    foreach (_testname ${ARGN})
        add_test ( ${_testname} python ${PROJECT_SOURCE_DIR}/../testsuite/${_testname}/run.py ${PROJECT_SOURCE_DIR}/../testsuite/${_testname} ${CMAKE_BINARY_DIR} )
    endforeach ()
endmacro ()

# List all the individual testsuite tests here, except those that need
# special installed tests.
TESTSUITE ( ico gpsread )

if (EXISTS "${PROJECT_SOURCE_DIR}/../../libtiffpic")
    TESTSUITE ( tiff-suite tiff-depths )
else ()
    message (STATUS "\n\nDid not find ${PROJECT_SOURCE_DIR}/../../libtiffpic")
    message (STATUS "  -> Will not run tests 'tiff-suite' and 'tiff-depths'")
    message (STATUS "  -> You can find it at http://www.remotesensing.org/libtiff/images.html\n")
endif ()

if (EXISTS "${PROJECT_SOURCE_DIR}/../../openexr-images-1.5.0")
    TESTSUITE ( openexr-suite openexr-multires openexr-chroma )
else ()
    message (STATUS "\n\nDid not find ${PROJECT_SOURCE_DIR}/../../openexr-images-1.5.0")
    message (STATUS "  -> Will not run tests 'openexr-suite', 'openexr-multires', 'openexr-chroma'")
    message (STATUS "  -> You can find it at http://www.openexr.com/downloads.html\n")
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
    add_dll_files ()
    include (InstallRequiredSystemLibraries)
endif ()
set (CPACK_SOURCE_PACKAGE_FILE_NAME OpenImageIO-${OpenImageIO_VERSION_MAJOR}.${OpenImageIO_VERSION_MINOR}.${OpenImageIO_VERSION_PATCH}-source)
#set (CPACK_SOURCE_STRIP_FILES Do we need this?)
set (CPACK_SOURCE_IGNORE_FILES ".*~")
set (CPACK_COMPONENT_UNSPECIFIED_HIDDEN TRUE)
set (CPACK_COMPONENT_UNSPECIFIED_REQUIRED TRUE)
set (CPACK_COMPONENTS_ALL user developer documentation Unspecified)
set (CPACK_COMPONENT_USER_DISPLAY_NAME "Applications")
set (CPACK_COMPONENT_DEVELOPER_DISPLAY_NAME "Developer files")
set (CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set (CPACK_COMPONENT_USER_DESCRIPTION
     "Applications: iv, iinfo, iconvert, idiff, igrep, maketx and libraries")
set (CPACK_COMPONENT_DEVELOPER_DESCRIPTION "Include files")
set (CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "OpenImageIO documentation")
set (CPACK_COMPONENT_DEVELOPER_DEPENDS user)
include (CPack)

# TODO: equivalents of the old:
#  * make doxygen
#  * BOOST_DYNAMIC

# Do TIFF, JPEG, PNG actually look in external?
