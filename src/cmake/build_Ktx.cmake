# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set_cache (Ktx_BUILD_VERSION v5.0.0-rc1 "Ktx version for local builds")
set (Ktx_GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git")
set (Ktx_GIT_TAG "${Ktx_BUILD_VERSION}")
set (Ktx_GIT_COMMIT "6269d2752ed04446c2d4749f54f3aad4f94555b5")
set_cache (Ktx_BUILD_SHARED_LIBS OFF ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local Ktx build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${Ktx_BUILD_VERSION} Ktx_VERSION_IDENT)

# for detailed build instructions, see:
#   https://github.com/KhronosGroup/KTX-Software/blob/main/BUILDING.md
# KTX-Software not only provides Ktx but also a set of cli tools and load
# test applications that we do not need.
build_dependency_with_cmake(Ktx
    VERSION         ${Ktx_BUILD_VERSION}
    GIT_REPOSITORY  ${Ktx_GIT_REPOSITORY}
    GIT_TAG         ${Ktx_GIT_TAG}
    GIT_COMMIT      ${Ktx_GIT_COMMIT}
    # lib only contains CMakeLists.txt from tag v5.0.0 but that requires CMake min version 3.23
    # which in turn causes the CI to fail. Just give up and build the whole thing...
    SOURCE_SUBDIR   lib  # To only build Ktx, cmake has to point to: KTX-Software/lib
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${Ktx_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D LIBKTX_VERSION_READ_ONLY=OFF
        -D LIBKTX_VERSION_FULL=ON
        -D LIBKTX_FEATURE_KTX1=ON  # Setting this to OFF causes linker issues
        -D LIBKTX_FEATURE_KTX2=ON
        -D LIBKTX_FEATURE_VK_UPLOAD=OFF
        -D LIBKTX_FEATURE_GL_UPLOAD=OFF
        -D LIBKTX_FEATURE_ETC_UNPACK=OFF # This has some weird licensing and I don't feel comfortable including it ...
        # as per KTX-Software:
        # > Intel Macs have support for SSE, but if you're building universal
        # > binaries, you have to disable SSE or the build will fail.
    )

# Set some things up that we'll need for a subsequent find_package to work
set (Ktx_ROOT ${Ktx_LOCAL_INSTALL_DIR})
set (Ktx_DIR ${Ktx_LOCAL_INSTALL_DIR}/lib/cmake/ktx)

# Signal to caller that we need to find again at the installed location
# set (Ktx_REFIND TRUE)
# set (Ktx_REFIND_ARGS CONFIG)

find_package (Ktx CONFIG REQUIRED
             HINTS 
                    ${Ktx_LOCAL_INSTALL_DIR}/lib/cmake/ktx/
                    ${Ktx_LOCAL_INSTALL_DIR}
             )

if (Ktx_BUILD_SHARED_LIBS)
  # install_local_dependency_libs (pkgname libname)
    install_local_dependency_libs (Ktx ktx) # notice libname is lowercase
endif ()
