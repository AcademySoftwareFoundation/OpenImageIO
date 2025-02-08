# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# fmt by hand!
######################################################################

set_cache (fmt_BUILD_VERSION 10.2.1 "fmt version for local builds")
set (fmt_GIT_REPOSITORY "https://github.com/fmtlib/fmt")
set (fmt_GIT_TAG "${fmt_BUILD_VERSION}")
# Note: fmt doesn't put "v" in front of version for its git tags

build_dependency_with_cmake(fmt
    VERSION         ${fmt_BUILD_VERSION}
    GIT_REPOSITORY  ${fmt_GIT_REPOSITORY}
    GIT_TAG         ${fmt_GIT_TAG}
    CMAKE_ARGS
        # -D CMAKE_INSTALL_LIBDIR=lib
        # Don't built unnecessary parts of fmt
        -D FMT_DOC=OFF
        -D FMT_TEST=OFF
    )

# Set some things up that we'll need for a subsequent find_package to work
set (fmt_ROOT ${fmt_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (fmt_REFIND TRUE)
