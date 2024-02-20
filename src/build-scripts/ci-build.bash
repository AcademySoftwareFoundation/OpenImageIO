#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

OIIO_CMAKE_FLAGS="$MY_CMAKE_FLAGS $OIIO_CMAKE_FLAGS"
export OIIO_SRC_DIR=${OIIO_SRC_DIR:=$PWD}
export OIIO_BUILD_DIR=${OIIO_BUILD_DIR:=${OIIO_SRC_DIR}/build}
export OIIO_INSTALL_DIR=${OIIO_INSTALL_DIR:=${OIIO_SRC_DIR}/dist}

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

# pushd $OIIO_BUILD_DIR
cmake -S $OIIO_SRC_DIR -B $OIIO_BUILD_DIR -G "$CMAKE_GENERATOR" \
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
mkdir ${OIIO_BUILD_DIR}/cmake-save || /bin/true
cp -r ${OIIO_BUILD_DIR}/CMake* ${OIIO_BUILD_DIR}/*.cmake ${OIIO_BUILD_DIR}/cmake-save

: ${BUILDTARGET:=install}
if [[ "$BUILDTARGET" != "none" ]] ; then
    echo "Parallel build ${CMAKE_BUILD_PARALLEL_LEVEL} of target ${BUILDTARGET}"
    if [[ ${OIIO_CMAKE_BUILD_WRAPPER} != "" ]] ; then
        echo "Using build wrapper '${OIIO_CMAKE_BUILD_WRAPPER}'"
    fi
    time ${OIIO_CMAKE_BUILD_WRAPPER} cmake --build ${OIIO_BUILD_DIR} --target ${BUILDTARGET} --config ${CMAKE_BUILD_TYPE}
fi
# popd

if [[ "${DEBUG_CI:=0}" != "0" ]] ; then
    echo "PATH=$PATH"
    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    echo "PYTHONPATH=$PYTHONPATH"
    echo "ldd oiiotool"
    ldd $OpenImageIO_ROOT/bin/oiiotool
fi

if [[ "$BUILDTARGET" == clang-format ]] ; then
    echo "Running " `which clang-format` " version " `clang-format --version`
    git diff --color
    THEDIFF=`git diff`
    if [[ "$THEDIFF" != "" ]] ; then
        echo "git diff was not empty. Failing clang-format or clang-tidy check."
        exit 1
    fi
fi
