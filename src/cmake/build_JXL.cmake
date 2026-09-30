# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# JPEG XL (libjxl) by hand!
######################################################################

set_cache (JXL_BUILD_VERSION 0.12.0 "libjxl version for local builds")
super_set (JXL_GIT_REPOSITORY "https://github.com/libjxl/libjxl")
super_set (JXL_GIT_TAG "v${JXL_BUILD_VERSION}")
super_set (JXL_GIT_COMMIT "a7a9c787341cf703dede03c2009fa460cae5e5df")

set_cache (JXL_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local JXL build, if necessary, build shared libraries" ADVANCED)

# A shared libjxl links its jxl_cms and brotli libraries, which are
# installed beside it, so point its rpath at its own directory.
if (APPLE)
    set (_jxl_rpath @loader_path)
else ()
    set (_jxl_rpath $ORIGIN)
endif ()

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
        -D CMAKE_INSTALL_RPATH=${_jxl_rpath}
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

# Signal to caller that we need to find again at the installed location
set (JXL_REFIND TRUE)
set (JXL_REFIND_VERSION ${JXL_BUILD_VERSION})
# FindJXL caches the threads library too, beyond the names the refind clears.
unset (JXL_THREADS_LIBRARY CACHE)

if (JXL_BUILD_SHARED_LIBS)
    # A shared libjxl also needs its shared jxl_cms and brotli libraries
    # (jxl_cms is matched by "jxl").
    install_local_dependency_libs (JXL jxl)
    install_local_dependency_libs (JXL brotli)
endif ()
