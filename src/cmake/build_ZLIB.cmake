# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# ZLIB by hand!
######################################################################

set_cache (ZLIB_BUILD_VERSION 1.3.1 "ZLIB version for local builds")
set (ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib")
set (ZLIB_GIT_TAG "v${ZLIB_BUILD_VERSION}")

build_dependency_with_cmake(ZLIB
    VERSION         ${ZLIB_BUILD_VERSION}
    GIT_REPOSITORY  ${ZLIB_GIT_REPOSITORY}
    GIT_TAG         ${ZLIB_GIT_TAG}
    )

# Set some things up that we'll need for a subsequent find_package to work
set (ZLIB_ROOT ${ZLIB_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (ZLIB_REFIND TRUE)
set (ZLIB_VERSION ${ZLIB_BUILD_VERSION})
