#!/usr/bin/env bash

# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if [[ ! -e build/$PLATFORM ]] ; then
    mkdir -p build/$PLATFORM
fi
if [[ ! -e dist/$PLATFORM ]] ; then
    mkdir -p dist/$PLATFORM
fi

# DEP_DIR="$PWD/ext/dist"
DEP_DIR="$PWD/dist/$PLATFORM"
mkdir -p "$DEP_DIR"
mkdir -p ext && true
INT_DIR="build/$PLATFORM"
VCPKG_INSTALLATION_ROOT=/c/vcpkg

export CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:=.}
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$DEP_DIR"
export BOOST_ROOT=${BOOST_ROOT_1_72_0}
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$BOOST_ROOT"
export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$VCPKG_INSTALLATION_ROOT/installed/x64-windows"
export PATH="$PATH:$DEP_DIR/bin:$DEP_DIR/lib:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin:/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$DEP_DIR/bin:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$DEP_DIR/lib:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/lib"

# export MY_CMAKE_FLAGS="$MY_CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
# export OPENEXR_CMAKE_FLAGS="$OPENEXR_CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

ls -l "C:/Program Files (x86)/Microsoft Visual Studio/*/Enterprise/VC/Tools/MSVC" && true
ls -l "C:/Program Files (x86)/Microsoft Visual Studio" && true


########################################################################
# Dependency method #1: Use vcpkg (disabled)
#
# Currently we are not using this, but here it is for reference:
#
# vcpkg list
# vcpkg update
# 
# # vcpkg install zlib:x64-windows
# vcpkg install tiff:x64-windows
# vcpkg install libpng:x64-windows
# vcpkg install giflib:x64-windows
# vcpkg install freetype:x64-windows
# # vcpkg install openexr:x64-windows
# # vcpkg install libjpeg-turbo:x64-windows
# 
# vcpkg install libraw:x64-windows
# vcpkg install openjpeg:x64-windows
# vcpkg install libsquish:x64-windows
# # vcpkg install ffmpeg:x64-windows   # takes FOREVER!
# # vcpkg install webp:x64-windows  # No such vcpkg package?a
# 
# #echo "$VCPKG_INSTALLATION_ROOT"
# #ls "$VCPKG_INSTALLATION_ROOT"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows/lib"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows/lib"
# #echo "$VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin"
# #ls "$VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin"
# 
# # export PATH="$PATH:$DEP_DIR/bin:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin"
# export PATH="$DEP_DIR/lib:$DEP_DIR/bin:$PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/lib"
# export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows/lib:$DEP_DIR/lib:$DEP_DIR/bin"
# 
# echo "All VCPkg installs:"
# vcpkg list
#
########################################################################


########################################################################
# Dependency method #2: Build from source ourselves
#
#

src/build-scripts/build_zlib.bash
export ZLIB_ROOT=$PWD/ext/dist

src/build-scripts/build_libpng.bash
export PNG_ROOT=$PWD/ext/dist

src/build-scripts/build_libtiff.bash
export TIFF_ROOT=$PWD/ext/dist

LIBJPEGTURBO_CONFIG_OPTS=-DWITH_SIMD=OFF
# ^^ because we're too lazy to build nasm
src/build-scripts/build_libjpeg-turbo.bash
export JPEGTurbo_ROOT=$PWD/ext/dist

source src/build-scripts/build_pybind11.bash
#export pybind11_ROOT=$PWD/ext/dist


# curl --location https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.2.1-win64-dev.zip -o ffmpeg-dev.zip
# unzip ffmpeg-dev.zip
# FFmpeg_ROOT=$PWD/ffmpeg-4.2.1-win64-dev

echo "CMAKE_PREFIX_PATH = $CMAKE_PREFIX_PATH"


OPENEXR_CXX_FLAGS=" /W1 /EHsc /DWIN32=1 "
#OPENEXR_BUILD_TYPE=$CMAKE_BUILD_TYPE
OPENEXR_INSTALL_DIR=$DEP_DIR
OPENEXR_VERSION=v2.4.1
source src/build-scripts/build_openexr.bash
export PATH="$OPENEXR_INSTALL_DIR/bin:$OPENEXR_INSTALL_DIR/lib:$PATH"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PATH
# the above line is admittedly sketchy

cp $DEP_DIR/lib/*.lib $DEP_DIR/bin
cp $DEP_DIR/bin/*.dll $DEP_DIR/lib
echo "DEP_DIR $DEP_DIR :"
ls -R -l "$DEP_DIR"


src/build-scripts/install_test_images.bash

ls /c/hostedtoolcache/windows/Python
echo "/c/hostedtoolcache/windows/Python/3.6.8/x64"
ls /c/hostedtoolcache/windows/Python/3.6.8/x64


# source src/build-scripts/build_openexr.bash
# export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$ILMBASE_ROOT;$OPENEXR_ROOT"
# source src/build-scripts/build_opencolorio.bash


if [[ "$PYTHON_VERSION" == "3.6" ]] ; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;/c/hostedtoolcache/windows/Python/3.6.8/x64"
fi
