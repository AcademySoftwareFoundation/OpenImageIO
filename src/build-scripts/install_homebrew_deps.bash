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

set -ex

if [[ "${DO_BREW_UPDATE:=0}" != "0" ]] ; then
    brew update >/dev/null
fi
echo ""
echo "Before my brew installs:"
brew list --versions

if [[ "$OIIO_BREW_INSTALL_PACKAGES" == "" ]] ; then
    OIIO_BREW_INSTALL_PACKAGES=" \
        ccache \
        dcmtk \
        ffmpeg \
        imath \
        libheif \
        libraw \
        libultrahdr \
        numpy \
        opencolorio \
        openexr \
        openjpeg \
        openvdb \
        ptex \
        pybind11 \
        robin-map \
        tbb \
        "
    if [[ "${USE_OPENCV:=}" != "0" ]] && [[ "${INSTALL_OPENCV:=1}" != "0" ]] ; then
        OIIO_BREW_INSTALL_PACKAGES+=" opencv"
    fi
    if [[ "${USE_QT:=1}" != "0" ]] && [[ "${INSTALL_QT:=1}" != "0" ]] ; then
        OIIO_BREW_INSTALL_PACKAGES+=" qt${QT_VERSION}"
    fi
fi
brew install --display-times -q $OIIO_BREW_INSTALL_PACKAGES $OIIO_BREW_EXTRA_INSTALL_PACKAGES || true

echo ""
echo "After brew installs:"
brew list --versions

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export PATH=/usr/local/opt/qt5/bin:$PATH
export PATH=/usr/local/opt/python/libexec/bin:$PATH
export PYTHONPATH=/usr/local/lib/python${PYTHON_VERSION}/site-packages:$PYTHONPATH

# Save the env for use by other stages
src/build-scripts/save-env.bash
