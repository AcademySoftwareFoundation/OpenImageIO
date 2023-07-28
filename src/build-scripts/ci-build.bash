#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

OIIO_CMAKE_FLAGS="$MY_CMAKE_FLAGS $OIIO_CMAKE_FLAGS"

if [[ "$USE_SIMD" != "" ]] ; then
    OIIO_CMAKE_FLAGS="$OIIO_CMAKE_FLAGS -DUSE_SIMD=$USE_SIMD"
fi

if [[ -n "$FMT_VERSION" ]] ; then
    OIIO_CMAKE_FLAGS="$OIIO_CMAKE_FLAGS -DBUILD_FMT_VERSION=$FMT_VERSION"
fi

if [[ -n "$CODECOV" ]] ; then
    OIIO_CMAKE_FLAGS="$OIIO_CMAKE_FLAGS -DCODECOV=${CODECOV}"
fi

# On GHA, we can reduce build time with "unity" builds.
if [[ ${GITHUB_ACTIONS} == true ]] ; then
    OIIO_CMAKE_FLAGS+=" -DCMAKE_UNITY_BUILD=${CMAKE_UNITY_BUILD:=ON} -DCMAKE_UNITY_BUILD_MODE=${CMAKE_UNITY_BUILD_MODE:=BATCH}"
fi

cmake -S . -B build -G "$CMAKE_GENERATOR" \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -DCMAKE_INSTALL_PREFIX="$OpenImageIO_ROOT" \
        -DPYTHON_VERSION="$PYTHON_VERSION" \
        -DCMAKE_INSTALL_LIBDIR="$OpenImageIO_ROOT/lib" \
        -DCMAKE_CXX_STANDARD="$CMAKE_CXX_STANDARD" \
        -DOIIO_DOWNLOAD_MISSING_TESTDATA=ON \
        -DEXTRA_CPP_ARGS="${OIIO_EXTRA_CPP_ARGS}" \
        $OIIO_CMAKE_FLAGS -DVERBOSE=1

# Save a copy of the generated files for debugging broken CI builds.
mkdir build/cmake-save || /bin/true
cp -r build/CMake* build/*.cmake build/cmake-save

: ${BUILDTARGET:=install}
if [[ "$BUILDTARGET" != "none" ]] ; then
    echo "Parallel build ${CMAKE_BUILD_PARALLEL_LEVEL} of target ${BUILDTARGET}"
    time ${OIIO_CMAKE_BUILD_WRAPPER} cmake --build build --target ${BUILDTARGET} --config ${CMAKE_BUILD_TYPE}
fi

if [[ "${DEBUG_CI:=0}" != "0" ]] ; then
    echo "PATH=$PATH"
    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    echo "PYTHONPATH=$PYTHONPATH"
    echo "ldd oiiotool"
    ldd $OpenImageIO_ROOT/bin/oiiotool
fi

if [[ "$BUILDTARGET" == clang-format ]] ; then
    git diff --color
    THEDIFF=`git diff`
    if [[ "$THEDIFF" != "" ]] ; then
        echo "git diff was not empty. Failing clang-format or clang-tidy check."
        exit 1
    fi
fi
