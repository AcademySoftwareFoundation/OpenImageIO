#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

DEP_DIR="$PWD/dist/$PLATFORM"
mkdir -p "$DEP_DIR"
mkdir -p ext
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


# In case we need vcpkg, example:
# echo "All pre-installed VCPkg installs:"
# vcpkg list
# echo "---------------"
# vcpkg update
# vcpkg install libdeflate:x64-windows-release
# export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH:$VCPKG_INSTALLATION_ROOT/installed/x64-windows-release"
#echo "All VCPkg installs:"
#vcpkg list



if [[ "$LIBTIFF_VERSION" != "" ]] ; then
    source src/build-scripts/build_libtiff.bash
fi

if [[ "$PYBIND11_VERSION" != "0" ]] ; then
    source src/build-scripts/build_pybind11.bash
fi


# curl --location https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.2.1-win64-dev.zip -o ffmpeg-dev.zip
# unzip ffmpeg-dev.zip
# FFmpeg_ROOT=$PWD/ffmpeg-4.2.1-win64-dev

echo "CMAKE_PREFIX_PATH = $CMAKE_PREFIX_PATH"


if [[ "$OPENEXR_VERSION" != "" ]] ; then
    OPENEXR_CXX_FLAGS=" /W1 /EHsc /DWIN32=1 "
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


# Save the env for use by other stages
src/build-scripts/save-env.bash
