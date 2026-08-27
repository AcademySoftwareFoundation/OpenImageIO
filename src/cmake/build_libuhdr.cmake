# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# libuhdr by hand!
######################################################################

set_cache (libuhdr_BUILD_VERSION 1.4.0 "libultrahdr version for local builds")
set (libuhdr_GIT_REPOSITORY "https://github.com/google/libultrahdr")
set (libuhdr_GIT_TAG "v${libuhdr_BUILD_VERSION}")
set (libuhdr_GIT_COMMIT "d52a0d13814ca399fc8a07e23de1d2c63f0e8404")

set_cache (libuhdr_BUILD_SHARED_LIBS OFF
           DOC "Should execute a local libuhdr build, if necessary, build shared libraries" ADVANCED)

if (TARGET libjpeg-turbo::jpeg)
    # We've had some trouble with libuhdr finding the JPEG resources it needs to
    # build if we're using libjpeg-turbo, libuhdr needs an extra nudge.
    get_target_property(JPEG_INCLUDE_DIR JPEG::JPEG INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(JPEG_LIBRARY JPEG::JPEG INTERFACE_LINK_LIBRARIES)
endif ()

set_cache (UHDR_CMAKE_C_COMPILER ${CMAKE_C_COMPILER} "libuhdr build C compiler override" ADVANCED)
set_cache (UHDR_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} "libuhdr build C++ compiler override" ADVANCED)

# libuhdr's own CMake install target does not work on Windows (it generates
# no install.vcxproj), so we skip CMake's install step there and instead
# manually place the built libs and header into the install dir below.
if (WIN32)
    set (_libuhdr_noinstall NOINSTALL)
else ()
    set (_libuhdr_noinstall)
endif ()

build_dependency_with_cmake(libuhdr
    VERSION         ${libuhdr_BUILD_VERSION}
    GIT_REPOSITORY  ${libuhdr_GIT_REPOSITORY}
    GIT_TAG         ${libuhdr_GIT_TAG}
    GIT_COMMIT      ${libuhdr_GIT_COMMIT}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${libuhdr_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D UHDR_BUILD_EXAMPLES=FALSE
        -D UHDR_BUILD_DEPS=FALSE
        -D UHDR_ENABLE_LOGS=TRUE
        -D JPEG_INCLUDE_DIR=${JPEG_INCLUDE_DIR}
        -D JPEG_LIBRARY=${JPEG_LIBRARY}
        -D CMAKE_C_COMPILER=${UHDR_CMAKE_C_COMPILER}
        -D CMAKE_CXX_COMPILER=${UHDR_CMAKE_CXX_COMPILER}
    ${_libuhdr_noinstall}
    )
unset (_libuhdr_noinstall)

if (WIN32)
    # The multi-config VS generator puts the .lib under a per-config
    # subdirectory named for the config we built (${..._DEPENDENCY_BUILD_TYPE},
    # e.g. MinSizeRel for our wheel builds), not always "Release".
    file (GLOB _lib_files "${libuhdr_LOCAL_BUILD_DIR}/${${PROJECT_NAME}_DEPENDENCY_BUILD_TYPE}/*.lib")
    file (COPY ${_lib_files} DESTINATION ${libuhdr_LOCAL_INSTALL_DIR}/lib)
    unset (_lib_files)
    file (GLOB _header_files "${libuhdr_LOCAL_SOURCE_DIR}/ultrahdr_api.h")
    file (COPY ${_header_files} DESTINATION ${libuhdr_LOCAL_INSTALL_DIR}/include)
    unset (_header_files)
    # build_dependency_with_cmake() normally sets these after a successful
    # install step; since we skipped that step above, set them by hand.
    set (libuhdr_ROOT ${libuhdr_LOCAL_INSTALL_DIR})
    list (APPEND CMAKE_PREFIX_PATH ${libuhdr_LOCAL_INSTALL_DIR})
endif ()

# Signal to caller that we need to find again at the installed location.
# libuhdr has no upstream CMake package config, only our own MODULE-mode
# Findlibuhdr.cmake, so REFIND_ARGS must not force CONFIG.
set (libuhdr_REFIND TRUE)
set (libuhdr_REFIND_VERSION ${libuhdr_BUILD_VERSION})

if (libuhdr_BUILD_SHARED_LIBS)
    install_local_dependency_libs (uhdr uhdr)
endif ()
