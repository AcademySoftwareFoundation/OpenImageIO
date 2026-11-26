# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# OpenEXR < 3.5.4 accepts only openjph 0.32.x and otherwise bundles its own
# copy, which collides with ours when both are linked statically.
set_cache (openjph_BUILD_VERSION 0.33.0 "openjph version for local builds")
set (openjph_GIT_REPOSITORY "https://github.com/aous72/OpenJPH.git")
set_cache (openjph_GIT_TAG "${openjph_BUILD_VERSION}" "Git branch or tag")
set_cache (openjph_GIT_COMMIT "345a8bcd1c7a3908e3870797057277660a412073"
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

# Signal to caller that we need to find again at the installed location
set (openjph_REFIND TRUE)
set (openjph_REFIND_VERSION ${openjph_BUILD_VERSION})
set (openjph_REFIND_ARGS CONFIG)

if (openjph_BUILD_SHARED_LIBS)
    install_local_dependency_libs (openjph openjph)
endif ()
