project (OpenImageIO)
set (OIIO_VERSION_MAJOR 1)
set (OIIO_VERSION_MINOR 1)
set (OIIO_VERSION_PATCH 0)

cmake_minimum_required (VERSION 2.6)
if (NOT CMAKE_VERSION VERSION_LESS 2.8.4)
    cmake_policy (SET CMP0017 NEW)
endif ()
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
set (USE_OPENJPEG ON CACHE BOOL "Use OpenJpeg if found")
set (USE_OCIO ON CACHE BOOL "Use OpenColorIO for color management if found")
set (USE_OPENCV ON CACHE BOOL "Use OpenCV if found")
set (NOTHREADS OFF CACHE BOOL "Compile with no threads or locking")
set (PYTHON_VERSION 2.6)
set (USE_EXTERNAL_PUGIXML OFF CACHE BOOL
     "Use an externally built shared library version of the pugixml library")

set (SOVERSION ${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}
     CACHE STRING "Set the SO version in the SO name of the output library")

if (NOTHREADS)
    message (STATUS "NO THREADS!")
    add_definitions ("-DNOTHREADS=1")
else ()
    message (STATUS "Building with thread support")
endif ()

if (USE_TBB)
    add_definitions ("-DUSE_TBB=1")
else ()
    add_definitions ("-DUSE_TBB=0")
    message (STATUS "TBB will not be used")
endif ()

set(CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/cmake/modules"
    "${PROJECT_SOURCE_DIR}/cmake")

# Set the default namespace
if(NOT OIIO_NAMESPACE)
  set(OIIO_NAMESPACE OpenImageIO CACHE STRING
      "Specify the master OpenImageIO C++ namespace: Options include OpenImageIO OpenImageIO_<YOURFACILITY> etc."
      FORCE)
endif(NOT OIIO_NAMESPACE)

message(STATUS "Setting Namespace to: ${OIIO_NAMESPACE}")


include (util_macros)
include (oiio_macros)
include (platform)
include (externalpackages)


include_directories(
    ${CMAKE_SOURCE_DIR}/include/
    ${CMAKE_BINARY_DIR}/include/
)

# Strict warnings
if (NOT MSVC)
    add_definitions ("-Wall")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # CMake doesn't automatically know what do do with
    # include_directories(SYSTEM...) when using clang.
    set (CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
    # Disable some warnings for Clang, for some things that are too awkward
    # to change just for the sake of having no warnings.
    add_definitions ("-Wno-unused-function")
    add_definitions ("-Wno-overloaded-virtual")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions ("-DDEBUG=1")
endif ()

###########################################################################
# Paths for install tree customization.  Note that relative paths are relative
# to CMAKE_INSTALL_PREFIX.
set (DEFAULT_BIN_INSTALL_DIR   "bin")
set (DEFAULT_LIB_INSTALL_DIR   "lib")
set (DEFAULT_INCLUDE_INSTALL_DIR "include/OpenImageIO")
if (UNIX AND NOT SELF_CONTAINED_INSTALL_TREE)
    # Try to be well-behaved and install into reasonable places according to
    # the "standard" unix directory heirarchy
    # TODO: Figure out how to get the correct python directory
    set (DEFAULT_PYLIB_INSTALL_DIR "lib/python/site-packages")
    set (DEFAULT_DOC_INSTALL_DIR "share/doc/openimageio")
    set (DEFAULT_MAN_INSTALL_DIR "share/man/man1")
else ()
    # Here is the "self-contained install tree" case: the expectation here is
    # that everything OIIO related will go into its own directory, not into
    # some standard system heirarchy.
    set (DEFAULT_PYLIB_INSTALL_DIR "python")
    set (DEFAULT_DOC_INSTALL_DIR "doc")
    set (DEFAULT_MAN_INSTALL_DIR "doc/man")
endif ()
if (EXEC_INSTALL_PREFIX)
    # Tack on an extra prefix to support multi-arch builds.
    set (DEFAULT_BIN_INSTALL_DIR   "${EXEC_INSTALL_PREFIX}/${DEFAULT_BIN_INSTALL_DIR}")
    set (DEFAULT_LIB_INSTALL_DIR   "${EXEC_INSTALL_PREFIX}/${DEFAULT_LIB_INSTALL_DIR}")
    set (DEFAULT_PYLIB_INSTALL_DIR "${EXEC_INSTALL_PREFIX}/${DEFAULT_PYLIB_INSTALL_DIR}")
endif ()
# Set up cmake cache variables corresponding to the defaults deduced above, so
# that the user can override them as desired:
set (BIN_INSTALL_DIR ${DEFAULT_BIN_INSTALL_DIR} CACHE STRING
     "Install location for binaries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (LIB_INSTALL_DIR ${DEFAULT_LIB_INSTALL_DIR} CACHE STRING
     "Install location for libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (PYLIB_INSTALL_DIR ${DEFAULT_PYLIB_INSTALL_DIR} CACHE STRING
     "Install location for python libraries (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (INCLUDE_INSTALL_DIR ${DEFAULT_INCLUDE_INSTALL_DIR} CACHE STRING
     "Install location of header files (relative to CMAKE_INSTALL_PREFIX or absolute)")
set (DOC_INSTALL_DIR ${DEFAULT_DOC_INSTALL_DIR} CACHE STRING
     "Install location for documentation (relative to CMAKE_INSTALL_PREFIX or absolute)")
if (UNIX)
    set (MAN_INSTALL_DIR ${DEFAULT_MAN_INSTALL_DIR} CACHE STRING
         "Install location for manual pages (relative to CMAKE_INSTALL_PREFIX or absolute)")
endif()

set (INSTALL_DOCS ON CACHE BOOL "Install documentation")


###########################################################################
# Rpath handling.
if (CMAKE_SKIP_RPATH)
    # We need to disallow the user from truly setting CMAKE_SKIP_RPATH, since
    # we want to run the generated executables from the build tree in order to
    # generate the manual page documentation.  However, we make sure the
    # install rpath is unset so that the install tree is still free of rpaths
    # for linux packaging purposes.
    set (CMAKE_SKIP_RPATH FALSE)
    unset (CMAKE_INSTALL_RPATH)
else ()
    set (CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR}")
    if (NOT IS_ABSOLUTE ${CMAKE_INSTALL_RPATH})
        set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
    endif ()
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif ()


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


# We want CTest for testing
# N.B. This needs to be added before any of the subdirectories, or
# their add_test commands will not register.
include (CTest)


# Tell CMake to process the sub-directories
add_subdirectory (libOpenImageIO)

add_subdirectory (iconvert)
add_subdirectory (idiff)
add_subdirectory (igrep)
add_subdirectory (iinfo)
add_subdirectory (iprocess)
add_subdirectory (maketx)
add_subdirectory (oiiotool)
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
    add_subdirectory (iff.imageio)
    add_subdirectory (jpeg.imageio)
    add_subdirectory (jpeg2000.imageio)
    add_subdirectory (openexr.imageio)
    add_subdirectory (png.imageio)
    add_subdirectory (pnm.imageio)
    add_subdirectory (psd.imageio)
    add_subdirectory (ptex.imageio)
    add_subdirectory (rla.imageio)
    add_subdirectory (sgi.imageio)
    add_subdirectory (socket.imageio)
    add_subdirectory (softimage.imageio)
    add_subdirectory (targa.imageio)
    add_subdirectory (tiff.imageio)
    add_subdirectory (webp.imageio)
    add_subdirectory (zfile.imageio)
endif ()

if (USE_PYTHON AND oiio_boost_PYTHON_FOUND)
    add_subdirectory (python)
endif ()

add_subdirectory (include)
add_subdirectory (doc)


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

# Make a copy of the testsuite into the build area
if (DEFINED CMAKE_VERSION AND NOT CMAKE_VERSION VERSION_LESS 2.8)
    file (COPY ${PROJECT_SOURCE_DIR}/../testsuite
          DESTINATION ${CMAKE_BINARY_DIR})
endif()

# List all the individual testsuite tests here:
oiio_add_tests (ico gpsread misnamed-file
                oiiotool oiiotool-fixnan 
                sgi rla psd dpx png
                texture-fill texture-gray texture-grid
                texture-missing texture-overscan
                texture-pointsample texture-width0blur texture-res
                texture-skinny texture-fat
               )


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
    FOUNDVAR OPENJPEG_FOUND
    IMAGEDIR j2kp4files_v1_5
    URL http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803)

oiio_add_tests (targa-tgautils
    IMAGEDIR TGAUTILS
    URL http://tgautils.inequation.org/)

oiio_add_tests (fits
    IMAGEDIR fits-images
    URL http://www.cv.nrao.edu/fits/data/tests/)

oiio_add_tests (webp
    FOUNDVAR WEBP_FOUND
    IMAGEDIR webp-images
    URL http://code.google.com/speed/webp/gallery.html)

oiio_add_tests (texture-field3d
    FOUNDVAR FIELD3D_FOUND)


#########################################################################
# Packaging
set (CPACK_PACKAGE_VERSION_MAJOR ${OIIO_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${OIIO_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${OIIO_VERSION_PATCH})
# "Vendor" is only used in copyright notices, so we use the same thing that
# the rest of the copyright notices say.
set (CPACK_PACKAGE_VENDOR "Larry Gritz et al.")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenImageIO is an open source library for reading and writing image file formats, a nice format-agnostic image viewer, and other image-related classes and utilities.")
set (CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/doc/Description.txt")
set (CPACK_PACKAGE_FILE_NAME OpenImageIO-${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}-${platform})
#SET (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_SOURCE_DIR}/..)
exec_program ("cmake -E copy ${PROJECT_SOURCE_DIR}/../LICENSE ${CMAKE_BINARY_DIR}/License.txt")
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/License.txt")
exec_program ("cmake -E copy ${PROJECT_SOURCE_DIR}/../README.rst ${CMAKE_BINARY_DIR}/Readme.rst")
set (CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/Readme.rst")
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
set (CPACK_SOURCE_PACKAGE_FILE_NAME OpenImageIO-${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}.${OIIO_VERSION_PATCH}-source)
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
