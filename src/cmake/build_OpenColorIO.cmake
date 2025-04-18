# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# OpenColorIO by hand!
######################################################################

set_cache (OpenColorIO_BUILD_VERSION 2.4.1 "OpenColorIO version for local builds")
set (OpenColorIO_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/OpenColorIO")
set (OpenColorIO_GIT_TAG "v${OpenColorIO_BUILD_VERSION}")
set_cache (OpenColorIO_BUILD_SHARED_LIBS  OFF #ON
           DOC "Should a local OpenColorIO build, if necessary, build shared libraries" ADVANCED)
# We would prefer to build a static OCIO, but haven't figured out how to make
# it all work with the static dependencies, it just makes things complicated
# downstream.

# Clear variables from the failed find_package
unset (OPENCOLORIO_LIBRARY)
unset (OPENCOLORIO_INCLUDE_DIR)
unset (FIND_PACKAGE_MESSAGE_DETAILS_OpenColorIO)
unset (OPENCOLORIO_VERSION_MAJOR)
unset (OPENCOLORIO_VERSION_MINOR)
unset (OpenColorIO_DIR)

checked_find_package(pystring VERSION_MIN 1.1.3)
checked_find_package(expat REQUIRED VERSION_MIN 2.5)
checked_find_package(yaml-cpp REQUIRED VERSION_MIN 0.6.0)
checked_find_package(minizip-ng REQUIRED VERSION_MIN 3.0.0)

string (MAKE_C_IDENTIFIER ${OpenColorIO_BUILD_VERSION} OpenColorIO_VERSION_IDENT)

build_dependency_with_cmake(OpenColorIO
    VERSION         ${OpenColorIO_BUILD_VERSION}
    GIT_REPOSITORY  ${OpenColorIO_GIT_REPOSITORY}
    GIT_TAG         ${OpenColorIO_GIT_TAG}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${OpenColorIO_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        # Don't built unnecessary parts of OCIO
        -D OCIO_BUILD_APPS=OFF
        -D OCIO_BUILD_GPU_TESTS=OFF
        -D OCIO_BUILD_PYTHON=OFF
        -D OCIO_BUILD_TESTS=OFF
        -D OCIO_USE_OIIO_FOR_APPS=OFF
        -D OCIO_INSTALL_DOCS=OFF
        # Make OCIO build all its dependencies statically
        -D OCIO_INSTALL_EXT_PACKAGES=MISSING
        # Give the library a custom name and symbol namespace so it can't
        # conflict with any others in the system or linked into the same app.
        # -D OCIO_NAMESPACE=${OpenColorIO_VERSION_IDENT}_${PROJ_NAME}
        -D OCIO_LIBNAME_SUFFIX=_v${OpenColorIO_VERSION_IDENT}_${PROJ_NAME}
        # Fix for yaml-cpp breaking against cmake 4.0.
        # Remove when yaml-cpp is fixed to declare its own minimum high enough.
        -D CMAKE_POLICY_VERSION_MINIMUM=3.5
    )

# Set some things up that we'll need for a subsequent find_package to work
set (OpenColorIO_ROOT ${OpenColorIO_LOCAL_INSTALL_DIR})
set (OpenColorIO_DIR ${OpenColorIO_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
# set (OpenColorIO_REFIND TRUE)
# set (OpenColorIO_REFIND_ARGS CONFIG)
find_package (OpenColorIO ${OpenColorIO_BUILD_VERSION} EXACT CONFIG REQUIRED)
find_package(pystring REQUIRED)

if (OpenColorIO_BUILD_SHARED_LIBS)
    install_local_dependency_libs (OpenColorIO OpenColorIO)
endif ()
