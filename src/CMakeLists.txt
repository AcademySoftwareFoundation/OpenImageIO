project (OpenImageIO)
set (OpenImageIO_VERSION_MAJOR 0)
set (OpenImageIO_VERSION_MINOR 7)
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

set (EMBEDPLUGINS ON CACHE BOOL "Embed format plugins in libOpenImageIO")
set (BUILDSTATIC OFF CACHE BOOL "Build static library instead of shared")
set (LINKSTATIC OFF CACHE BOOL "Link with static external libraries when possible")
set (USE_OPENGL ON CACHE BOOL "Include OpenGL support")
set (USE_QT ON CACHE BOOL "Include Qt support")
set (FORCE_OPENGL_1 OFF CACHE BOOL "Force iv to use OpenGL's fixed pipeline")
set (USE_TBB ON CACHE BOOL "Use TBB if needed")
set (USE_PYTHON ON CACHE BOOL "Build the Python bindings")
set (USE_FIELD3D ON CACHE BOOL "Use Field3D if found")
set (NOTHREADS OFF CACHE BOOL "Compile with no threads or locking")
set (SOVERSION ${OpenImageIO_VERSION_MAJOR} CACHE STRING "Set the SO version in the SO name of the output library")

set (PYTHON_VERSION 2.6)

if (NOT ${SOVERSION} STREQUAL ${OpenImageIO_VERSION_MAJOR})
    set (OpenImageIO_VERSION_MAJOR ${SOVERSION})
endif ()

if (NOTHREADS)
    message (STATUS "NO THREADS!")
    add_definitions ("-DNOTHREADS=1")
else ()
    message (STATUS "Building with thread support")
endif ()

set(CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/cmake/modules"
    "${PROJECT_SOURCE_DIR}/cmake")

include (util_macros)
include (oiio_macros)
include (platform)
include (externalpackages)


# We want CTest for testing
include (CTest)

include_directories ("include")

# Strict warnings
if (NOT WIN32)
    add_definitions ("-Wall")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions ("-DDEBUG=1")
endif ()

if (OPENIMAGEIO_NAMESPACE)
    add_definitions ("-DOPENIMAGEIO_NAMESPACE=${OPENIMAGEIO_NAMESPACE}")
endif ()

# Exec Install Locations
set (BINDIR   "${CMAKE_INSTALL_PREFIX}/bin")
set (LIBDIR   "${CMAKE_INSTALL_PREFIX}/lib")
set (PYLIBDIR "${CMAKE_INSTALL_PREFIX}/python")
if (EXEC_INSTALL_PREFIX)
    set (BINDIR   "${EXEC_INSTALL_PREFIX}/bin")
    set (LIBDIR   "${EXEC_INSTALL_PREFIX}/lib")
    set (PYLIBDIR "${EXEC_INSTALL_PREFIX}/python")
endif ()

set (CMAKE_INSTALL_RPATH "${LIBDIR}")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

###########################################################################
if (MSVC)
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS)
    add_definitions (-DJAS_WIN_MSVC_BUILD)
    if(NOT LINKSTATIC)
        add_definitions (-DBOOST_ALL_DYN_LINK)
        add_definitions (-DOPENEXR_DLL)
    endif ()
    if (BUILDSTATIC)
         add_definitions(-DOIIO_STATIC_BUILD=1)
    endif ()
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
    add_subdirectory (cineon.imageio)
    add_subdirectory (dds.imageio)
    add_subdirectory (dpx.imageio)
    add_subdirectory (field3d.imageio)
    add_subdirectory (fits.imageio)
    add_subdirectory (hdr.imageio)
    add_subdirectory (ico.imageio)
    add_subdirectory (jpeg.imageio)
    add_subdirectory (jpeg2000.imageio)
    add_subdirectory (openexr.imageio)
    add_subdirectory (png.imageio)
    add_subdirectory (pnm.imageio)
    add_subdirectory (ptex.imageio)
    add_subdirectory (sgi.imageio)
    add_subdirectory (socket.imageio)
    add_subdirectory (softimage.imageio)
    add_subdirectory (targa.imageio)
    add_subdirectory (tiff.imageio)
    add_subdirectory (zfile.imageio)
endif ()

if (USE_PYTHON)
    add_subdirectory (python)
endif ()

add_subdirectory (include)
add_subdirectory (doc)


#########################################################################
# Testing

# List all the individual testsuite tests here:
oiio_add_tests (ico gpsread sgi texture-grid texture-missing texture-res
                imagecache-files texture-field3d)

# List testsuites which need special external reference images from the web
# here:
oiio_add_tests (bmp
    IMAGEDIR bmpsuite
    URL http://entropymine.com/jason/bmpsuite/bmpsuite.zip)

oiio_add_tests (tiff-suite tiff-depths
    IMAGEDIR libtiffpic
    URL http://www.remotesensing.org/libtiff/images.html)

oiio_add_tests (openexr-suite openexr-multires openexr-chroma
    IMAGEDIR openexr-images-1.5.0
    URL http://www.openexr.com/downloads.html)

oiio_add_tests (jpeg2000
    IMAGEDIR j2kp4files_v1_5
    URL http://www.crc.ricoh.com/~gormish/jpeg2000conformance/)

oiio_add_tests (targa-tgautils
    IMAGEDIR TGAUTILS
    URL http://tgautils.inequation.org/)

oiio_add_tests (fits
    IMAGEDIR fits-images
    URL http://www.cv.nrao.edu/fits/data/tests/)

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
