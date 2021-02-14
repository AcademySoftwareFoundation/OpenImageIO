#!/usr/bin/env bash

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

if [[ "$USE_SIMD" != "" ]] ; then
    MY_CMAKE_FLAGS="$MY_CMAKE_FLAGS -DUSE_SIMD=$USE_SIMD"
fi

pushd build/$PLATFORM
cmake ../.. -G "$CMAKE_GENERATOR" \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -DCMAKE_INSTALL_PREFIX="$OpenImageIO_ROOT" \
        -DPYTHON_VERSION="$PYTHON_VERSION" \
        -DCMAKE_INSTALL_LIBDIR="$OpenImageIO_ROOT/lib" \
        -DCMAKE_CXX_STANDARD="$CMAKE_CXX_STANDARD" \
        $MY_CMAKE_FLAGS -DVERBOSE=1
if [[ "$BUILDTARGET" != "none" ]] ; then
    echo "Parallel build " ${CMAKE_BUILD_PARALLEL_LEVEL}
    time cmake --build . --target ${BUILDTARGET:=install} --config ${CMAKE_BUILD_TYPE}
fi
popd

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
