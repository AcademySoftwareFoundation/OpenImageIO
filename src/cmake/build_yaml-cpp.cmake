# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# yaml-cpp by hand!
######################################################################

set_cache (yaml-cpp_BUILD_VERSION 0.8.0 "yaml-cpp version for local builds")
set (yaml-cpp_GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp")
set (yaml-cpp_GIT_TAG "${yaml-cpp_BUILD_VERSION}") # NB: versions earlier than 0.8.0 had a "yaml-cpp-" prefix

set_cache (yaml-cpp_BUILD_SHARED_LIBS OFF
           DOC "Should a local yaml-cpp build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${yaml-cpp_BUILD_VERSION} yaml-cpp_VERSION_IDENT)

build_dependency_with_cmake(yaml-cpp
    VERSION         ${yaml-cpp_BUILD_VERSION}
    GIT_REPOSITORY  ${yaml-cpp_GIT_REPOSITORY}
    GIT_TAG         ${yaml-cpp_GIT_TAG}
    CMAKE_ARGS
        -D YAML_CPP_BUILD_TESTS=OFF
        -D YAML_CPP_BUILD_TOOLS=OFF
        -D YAML_CPP_BUILD_CONTRIB=OFF
        -D YAML_BUILD_SHARED_LIBS=${yaml-cpp_BUILD_SHARED_LIBS}
        -D CMAKE_INSTALL_LIBDIR=lib
    )

set (yaml-cpp_ROOT ${yaml-cpp_LOCAL_INSTALL_DIR})
set (yaml-cpp_DIR ${yaml-cpp_LOCAL_INSTALL_DIR})
set (yaml-cpp_VERSION ${yaml-cpp_BUILD_VERSION})

set (yaml-cpp_REFIND TRUE)
set (yaml-cpp_REFIND_ARGS CONFIG)
set (yaml-cpp_REFIND_VERSION ${yaml-cpp_BUILD_VERSION})


if (yaml-cpp_BUILD_SHARED_LIBS)
    install_local_dependency_libs (yaml-cpp yaml-cpp)
endif ()