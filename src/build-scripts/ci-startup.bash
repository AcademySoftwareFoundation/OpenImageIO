#!/usr/bin/env bash

# This script is run when CI system first starts up.
# Since it sets many env variables needed by the caller, it should be run
# with 'source', not in a separate shell.

# Figure out the platform
if [[ $$RUNNER_OS == macOS ]] ; then
    export ARCH=macosx
elif [[ $RUNNER_OS == Linux ]] ; then
    export ARCH=linux64
elif [[ $RUNNER_OS == Windows ]] ; then
    export ARCH=windows64
elif [[ `uname -m` == aarch64 ]] ; then
    export ARCH=aarch64
else
    export ARCH=unknown
fi
export PLATFORM=$ARCH

if [[ "${DEBUG:=0}" != "0" ]] ; then
    export PLATFORM=${PLATFORM}.debug
fi

echo "Architecture is $ARCH"
echo "Build platform name is $PLATFORM"

# Environment variables we always need
export PATH=/usr/local/bin/_ccache:/usr/lib/ccache:$PATH
export USE_CCACHE=${USE_CCACHE:=1}
export CCACHE_CPP2=
export CCACHE_DIR=/tmp/ccache
if [[ "${RUNNER_OS}" == "macOS" ]] ; then
    export CCACHE_DIR=$HOME/.ccache
fi
mkdir -p $CCACHE_DIR

export OpenImageIO_ROOT=$PWD/dist/$PLATFORM
export DYLD_LIBRARY_PATH=$OpenImageIO_ROOT/lib:$DYLD_LIBRARY_PATH
export LD_LIBRARY_PATH=$OpenImageIO_ROOT/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$OpenImageIO_ROOT/lib64:$LD_LIBRARY_PATH
export OIIO_LIBRARY_PATH=$OpenImageIO_ROOT/lib
export LSAN_OPTIONS=suppressions=$PWD/src/build-scripts/nosanitize.txt
export ASAN_OPTIONS=print_suppressions=0

export PYTHON_VERSION=${PYTHON_VERSION:="2.7"}
export PYTHONPATH=$OpenImageIO_ROOT/lib/python${PYTHON_VERSION}/site-packages:$PYTHONPATH
export BUILD_MISSING_DEPS=${BUILD_MISSING_DEPS:=1}
export COMPILER=${COMPILER:=gcc}
export CXX=${CXX:=g++}
export CI=true
export USE_NINJA=${USE_NINJA:=1}
export CMAKE_GENERATOR=${CMAKE_GENERATOR:=Ninja}
export CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:=Release}
export CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD:=11}

export PARALLEL=${PARALLEL:=4}
export PAR_MAKEFLAGS=-j${PARALLEL}
export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:=${PARALLEL}}
export CTEST_PARALLEL_LEVEL=${CTEST_PARALLEL_LEVEL:=${PARALLEL}}

export LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=$HOME/ext}
export PATH=${LOCAL_DEPS_DIR}/dist/bin:$PATH
export LD_LIBRARY_PATH=${LOCAL_DEPS_DIR}/dist/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${LOCAL_DEPS_DIR}/dist/lib64:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=${LOCAL_DEPS_DIR}/dist/lib:$DYLD_LIBRARY_PATH

export OCIO="$PWD/testsuite/common/OpenColorIO/nuke-default/config.ocio"
export TESTSUITE_CLEANUP_ON_SUCCESS=${TESTSUITE_CLEANUP_ON_SUCCESS:=1}

mkdir -p build/$PLATFORM dist/$PLATFORM

echo "HOME = $HOME"
echo "PWD = $PWD"
echo "LOCAL_DEPS_DIR = $LOCAL_DEPS_DIR"
echo "uname -a: " `uname -a`
echo "uname -m: " `uname -m`
echo "uname -s: " `uname -s`
echo "uname -n: " `uname -n`
pwd
ls
env | sort

if [[ `uname -s` == "Linux" ]] ; then
    head -40 /proc/cpuinfo
elif [[ $ARCH == macosx ]] ; then
    sysctl machdep.cpu.features
fi

# Save the env for use by other stages
src/build-scripts/save-env.bash
