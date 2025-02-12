#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# DEP_DIR="$PWD/ext/dist"
DEP_DIR="$PWD/dist/$PLATFORM"
mkdir -p "$DEP_DIR"
mkdir -p ext && true
INT_DIR="build/$PLATFORM"
VCPKG_INSTALLATION_ROOT=/c/vcpkg

export CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:=.}
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$DEP_DIR"
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release"
export PATH="$PATH:$DEP_DIR/bin:$DEP_DIR/lib:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/bin:/bin:$PWD/ext/dist/bin:$PWD/ext/dist/lib"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$DEP_DIR/bin:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$DEP_DIR/lib:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/lib"

# export MY_CMAKE_FLAGS="$MY_CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
# export OPENEXR_CMAKE_FLAGS="$OPENEXR_CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

#ls -l "C:/Program Files (x86)/Microsoft Visual Studio/*/Enterprise/VC/Tools/MSVC" && true
#ls -l "C:/Program Files (x86)/Microsoft Visual Studio" && true


if [[ "$PYTHON_VERSION" == "3.7" ]] ; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;/c/hostedtoolcache/windows/Python/3.7.9/x64"
    export Python_EXECUTABLE="/c/hostedtoolcache/windows/Python/3.7.9/x64/python.exe"
    export PYTHONPATH=$OpenImageIO_ROOT/lib/python${PYTHON_VERSION}/site-packages
elif [[ "$PYTHON_VERSION" == "3.9" ]] ; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;/c/hostedtoolcache/windows/Python/3.9.13/x64"
    export Python_EXECUTABLE="/c/hostedtoolcache/windows/Python/3.9.13/x64/python3.exe"
    export PYTHONPATH=$OpenImageIO_ROOT/lib/python${PYTHON_VERSION}/site-packages
fi
pip install numpy


########################################################################
# Dependency method #1: Use vcpkg (disabled)
#
# Currently we are not using this, but here it is for reference:
#
echo "All pre-installed VCPkg installs:"
vcpkg list
echo "---------------"
# vcpkg update
# 

#vcpkg install libdeflate:x64-windows-release
#vcpkg install zlib:x64-windows-release
vcpkg install tiff:x64-windows-release
# vcpkg install libpng:x64-windows-release
# vcpkg install giflib:x64-windows-release
vcpkg install freetype:x64-windows-release
# # vcpkg install openexr:x64-windows-release
vcpkg install libjpeg-turbo:x64-windows-release
# 
# vcpkg install libraw:x64-windows-release
# vcpkg install openjpeg:x64-windows-release
# # vcpkg install ffmpeg:x64-windows-release   # takes FOREVER!
# # vcpkg install webp:x64-windows-release  # No such vcpkg package?a
# 
# #echo "$VCPKG_INSTALLATION_ROOT"
# #ls "$VCPKG_INSTALLATION_ROOT"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/lib"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/lib"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/bin"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/bin"
# 
# # export PATH="$PATH:$DEP_DIR/bin:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/bin"
# export PATH="$DEP_DIR/lib:$DEP_DIR/bin:$PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/lib"
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release"
# export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release/lib:$DEP_DIR/lib:$DEP_DIR/bin"
# 
echo "All VCPkg installs:"
vcpkg list
#
########################################################################


########################################################################
# Dependency method #2: Build from source ourselves
#
#

# src/build-scripts/build_zlib.bash
# export ZLIB_ROOT=$PWD/ext/dist

# src/build-scripts/build_libpng.bash
# export PNG_ROOT=$PWD/ext/dist

# We're currently getting libtiff from vcpkg
src/build-scripts/build_libtiff.bash
export TIFF_ROOT=$PWD/ext/dist

# We're currently getting jpeg from vcpkg
# LIBJPEGTURBO_CONFIG_OPTS=-DWITH_SIMD=OFF
# # ^^ because we're too lazy to build nasm
# src/build-scripts/build_libjpeg-turbo.bash
# export JPEGTurbo_ROOT=$PWD/ext/dist

source src/build-scripts/build_pybind11.bash
#export pybind11_ROOT=$PWD/ext/dist


# curl --location https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.2.1-win64-dev.zip -o ffmpeg-dev.zip
# unzip ffmpeg-dev.zip
# FFmpeg_ROOT=$PWD/ffmpeg-4.2.1-win64-dev

echo "CMAKE_PREFIX_PATH = $CMAKE_PREFIX_PATH"


if [[ "$OPENEXR_VERSION" != "" ]] ; then
    OPENEXR_CXX_FLAGS=" /W1 /EHsc /DWIN32=1 "
    #OPENEXR_BUILD_TYPE=$CMAKE_BUILD_TYPE
    OPENEXR_INSTALL_DIR=$DEP_DIR
    source src/build-scripts/build_openexr.bash
    export PATH="$OPENEXR_INSTALL_DIR/bin:$OPENEXR_INSTALL_DIR/lib:$PATH"
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PATH
    # the above line is admittedly sketchy
fi

cp $DEP_DIR/lib/*.lib $DEP_DIR/bin || true
cp $DEP_DIR/bin/*.dll $DEP_DIR/lib || true
echo "DEP_DIR $DEP_DIR :"
ls -R -l "$DEP_DIR"


# source src/build-scripts/build_openexr.bash
# export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$OPENEXR_ROOT"
# source src/build-scripts/build_opencolorio.bash


# Save the env for use by other stages
src/build-scripts/save-env.bash
