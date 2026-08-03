# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# JPEG XL (libjxl) by hand!
######################################################################

set_cache (JXL_BUILD_VERSION 0.12.0 "libjxl version for local builds")
set (JXL_GIT_REPOSITORY "https://github.com/libjxl/libjxl")
set_cache (JXL_GIT_TAG "v${JXL_BUILD_VERSION}" "Git branch or tag")
set_cache (JXL_GIT_COMMIT "a7a9c787341cf703dede03c2009fa460cae5e5df"
           "commit hash to verify tag against")

# Build libjxl shared: its bundled brotli/highway/skcms are built static
# and folded in, so the installed libraries are self-contained and match
# what OIIO's FindJXL module expects to link (jxl + jxl_threads).
set_cache (JXL_BUILD_SHARED_LIBS ON
           DOC "Should a local JXL build, if necessary, build shared libraries" ADVANCED)

build_dependency_with_cmake(JXL
    VERSION         ${JXL_BUILD_VERSION}
    GIT_REPOSITORY  ${JXL_GIT_REPOSITORY}
    GIT_TAG         ${JXL_GIT_TAG}
    GIT_COMMIT      ${JXL_GIT_COMMIT}
    # libjxl vendors its required dependencies as submodules. Only these
    # three are needed for the libraries themselves; skcms doubles as the
    # color-management backend so no system lcms2 is required.
    GIT_SUBMODULES  third_party/brotli third_party/highway third_party/skcms
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${JXL_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        # Libraries only -- no tools, tests, docs, or bindings.
        -D BUILD_TESTING=OFF
        -D JPEGXL_ENABLE_TOOLS=OFF
        -D JPEGXL_ENABLE_EXAMPLES=OFF
        -D JPEGXL_ENABLE_BENCHMARK=OFF
        -D JPEGXL_ENABLE_MANPAGES=OFF
        -D JPEGXL_ENABLE_DOXYGEN=OFF
        -D JPEGXL_ENABLE_JNI=OFF
        -D JPEGXL_ENABLE_FUZZERS=OFF
        # No optional integrations: OIIO's plugin uses the core codestream
        # API only. (Transcoding to/from legacy JPEG and the sjpeg/OpenEXR
        # helpers are tool/library features OIIO doesn't touch.)
        -D JPEGXL_ENABLE_SJPEG=OFF
        -D JPEGXL_ENABLE_OPENEXR=OFF
        -D JPEGXL_ENABLE_TRANSCODE_JPEG=OFF
        # Color management via the bundled skcms submodule.
        -D JPEGXL_ENABLE_SKCMS=ON
    )

# Set some things up that we'll need for a subsequent find_package to work

set (JXL_ROOT ${JXL_LOCAL_INSTALL_DIR})

# Signal to caller that we need to find again at the installed location
set (JXL_REFIND TRUE)

if (JXL_BUILD_SHARED_LIBS)
    install_local_dependency_libs (JXL jxl)
endif ()
