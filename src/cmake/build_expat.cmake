# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

######################################################################
# expat by hand!
######################################################################

set_cache (expat_BUILD_VERSION 2.6.3 "expat version for local builds")
set (expat_GIT_REPOSITORY "https://github.com/libexpat/libexpat")

string(REPLACE "." ";" VERSION_LIST ${expat_BUILD_VERSION})
list(GET VERSION_LIST 0 expat_VERSION_MAJOR)
list(GET VERSION_LIST 1 expat_VERSION_MINOR)
list(GET VERSION_LIST 2 expat_VERSION_PATCH)
        
set (expat_GIT_TAG "R_${expat_VERSION_MAJOR}_${expat_VERSION_MINOR}_${expat_VERSION_PATCH}")
set_cache (expat_BUILD_SHARED_LIBS OFF #${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should execute a local expat build, if necessary, build shared libraries" ADVANCED)


set (expat_LOCAL_SOURCE_DIR "${${PROJECT_NAME}_LOCAL_DEPS_ROOT}/expat/expat")

build_dependency_with_cmake(expat
    VERSION         ${expat_BUILD_VERSION}
    GIT_REPOSITORY  ${expat_GIT_REPOSITORY}
    GIT_TAG         ${expat_GIT_TAG}
    SOURCE_SUBDIR   expat/
    CMAKE_ARGS
        -D BUILD_SHARED_LIBS=${expat_BUILD_SHARED_LIBS}
        -D CMAKE_POSITION_INDEPENDENT_CODE=ON
        -D CMAKE_INSTALL_LIBDIR=lib
        -D CMAKE_C_FLAGS=${EXPAT_C_FLAGS}
        -D CMAKE_CXX_FLAGS=${EXPAT_CXX_FLAGS}
        -D EXPAT_BUILD_EXAMPLES=OFF
        -D EXPAT_BUILD_TESTS=OFF
        -D EXPAT_BUILD_TOOLS=OFF
        -D EXPAT_BUILD_DOCS=OFF
        -D EXPAT_SHARED_LIBS=${expat_BUILD_SHARED_LIBS}
    )

# Set some things up that we'll need for a subsequent find_package to work
set (expat_REFIND TRUE)
set (expat_VERSION ${expat_BUILD_VERSION})
set (expat_DIR ${expat_ROOT}/lib/cmake/expat-${expat_VERSION})

if (WIN32)
    # Set the expat_LIBRARY variable to the full path to ${EXPAT_LIBRARIES}.
    # For some reason, find_package(expat) behaves differently on Windows
    find_package (EXPAT ${expat_BUILD_VERSION} EXACT REQUIRED)
    set_cache(expat_LIBRARY ${EXPAT_LIBRARIES} "Full path to the expat library")
    message(STATUS "expat_LIBRARY = ${expat_LIBRARY}")
endif ()

if (expat_BUILD_SHARED_LIBS)
    install_local_dependency_libs (expat expat)
endif ()
