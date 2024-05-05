#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# This script, which assumes it is running on a Mac OSX with Homebrew
# installed, does a "brew install" in all packages reasonably needed by
# OIIO.

if [[ `uname` != "Darwin" ]] ; then
    echo "Don't run this script unless you are on Mac OSX"
    exit 1
fi

if [[ `which brew` == "" ]] ; then
    echo "You need to install Homebrew before running this script."
    echo "See http://brew.sh"
    exit 1
fi


if [[ "${DO_BREW_UPDATE:=0}" != "0" ]] ; then
    brew update >/dev/null
fi
echo ""
echo "Before my brew installs:"
brew list --versions

# All cases except for clang-format target, we need the dependencies.
brew install --display-times -q gcc ccache cmake ninja boost || true
brew link --overwrite gcc
brew install --display-times -q python@${PYTHON_VERSION} || true
brew unlink python@3.8 || true
brew unlink python@3.9 || true
brew unlink python@3.10 || true
brew link --overwrite --force python@${PYTHON_VERSION} || true
#brew upgrade --display-times -q cmake || true
#brew install --display-times -q libtiff
brew install --display-times -q imath openexr opencolorio
#brew install --display-times -q libpng giflib webp
brew install --display-times -q jpeg-turbo openjpeg
brew install --display-times -q freetype libraw dcmtk pybind11 numpy || true
brew install --display-times -q ffmpeg libheif ptex || true
brew install --display-times -q tbb || true
brew install --display-times -q openvdb || true
if [[ "${USE_OPENCV}" != "0" ]] ; then
    brew install --display-times -q opencv || true
fi
if [[ "${USE_QT}" != "0" ]] ; then
    brew install --display-times -q qt${QT_VERSION}
fi
if [[ "${USE_LLVM:=0}" != "0" ]] || [[ "${LLVMBREWVER}" != "" ]]; then
    brew install --display-times -q llvm${LLVMBREWVER}
    export PATH=/usr/local/opt/llvm/bin:$PATH
fi

echo ""
echo "After brew installs:"
brew list --versions

# Needed on some systems
pip${PYTHON_VERSION} install numpy

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export PATH=/usr/local/opt/qt5/bin:$PATH
export PATH=/usr/local/opt/python/libexec/bin:$PATH
export PYTHONPATH=/usr/local/lib/python${PYTHON_VERSION}/site-packages:$PYTHONPATH

# Save the env for use by other stages
src/build-scripts/save-env.bash
