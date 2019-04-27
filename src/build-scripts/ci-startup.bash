#!/usr/bin/env bash

# This script is run when CI system first starts up.
# Since it sets many env variables needed by the caller, it should be run
# with 'source', not in a separate shell.

# Figure out the platform
if [[ $TRAVIS_OS_NAME == osx ]] ; then
      export ARCH=macosx
fi
if [[ $TRAVIS_OS_NAME == linux || $CIRCLECI == true ]] ; then
      export ARCH=linux64
fi
PLATFORM=$ARCH

if [[ "$DEBUG" == 1 ]] ; then
    export PLATFORM=${PLATFORM}.debug
fi

echo "Architecture is $ARCH"
echo "Build platform name is $PLATFORM"

# Environment variables we always need
export USE_CCACHE=1
export CCACHE_CPP2=1
export OPENIMAGEIO_ROOT_DIR=$PWD/dist/$PLATFORM
export DYLD_LIBRARY_PATH=$OPENIMAGEIO_ROOT_DIR/lib:$DYLD_LIBRARY_PATH
export LD_LIBRARY_PATH=$OPENIMAGEIO_ROOT_DIR/lib:$LD_LIBRARY_PATH
export OIIO_LIBRARY_PATH=$OPENIMAGEIO_ROOT_DIR/lib
export PYTHONPATH=$OPENIMAGEIO_ROOT_DIR/python:$PYTHONPATH
export LSAN_OPTIONS=suppressions=$PWD/src/build-scripts/nosanitize.txt
export ASAN_OPTIONS=print_suppressions=0

export PYTHON_VERSION=${PYTHON_VERSION:="2.7"}
export BUILD_MISSING_DEPS=${BUILD_MISSING_DEPS:=1}
export COMPILER=${COMPILER:=gcc}
export CXX=${CXX:=g++}

uname -n
pwd
ls
env

if [[ $ARCH == linux64 ]] ; then
    head -40 /proc/cpuinfo
elif [[ $ARCH == macosx ]] ; then
    sysctl machdep.cpu.features
fi

