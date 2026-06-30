# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set_cache (openjph_BUILD_VERSION 0.30.1 "openjph version for local builds")
set (openjph_GIT_REPOSITORY "https://github.com/aous72/OpenJPH.git")
set (openjph_GIT_TAG "${openjph_BUILD_VERSION}")
set_cache (openjph_GIT_COMMIT "1ce857c7f14dd78dd8f4bdfabe43dc17f8408a42"
           "commit hash to verify tag against")
set_cache (openjph_BUILD_SHARED_LIBS ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local openjph build, if necessary, build shared libraries" ADVANCED)
set_cache (openjph_CMAKE_C_COMPILER ${CMAKE_C_COMPILER} "libopenjph build C compiler override" ADVANCED)
set_cache (openjph_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} "libopenjph build C++ compiler override" ADVANCED)


string (MAKE_C_IDENTIFIER ${openjph_BUILD_VERSION} openjph_VERSION_IDENT)

build_dependency_with_cmake(openjph
    VERSION         ${openjph_BUILD_VERSION}
    GIT_REPOSITORY  ${openjph_GIT_REPOSITORY}
    GIT_TAG         ${openjph_GIT_TAG}
    GIT_COMMIT      ${openjph_GIT_COMMIT}
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${openjph_BUILD_SHARED_LIBS}
        -D OJPH_BUILD_EXECUTABLES=OFF
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_C_COMPILER=${openjph_CMAKE_C_COMPILER}
        -D CMAKE_CXX_COMPILER=${openjph_CMAKE_CXX_COMPILER}
    )

# Set some things up that we'll need for a subsequent find_package to work
set (openjph_ROOT ${openjph_LOCAL_INSTALL_DIR})
set (openjph_VERSION ${openjph_BUILD_VERSION})

# Signal to caller that we need to find again at the installed location
set (openjph_REFIND TRUE)
set (openjph_REFIND_VERSION ${openjph_BUILD_VERSION})
set (openjph_REFIND_ARGS CONFIG)

if (openjph_BUILD_SHARED_LIBS)
    install_local_dependency_libs (openjph openjph)
endif ()
