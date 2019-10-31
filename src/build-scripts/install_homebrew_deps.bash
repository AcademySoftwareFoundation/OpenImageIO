#!/bin/bash

# This script, which assumes it is runnign on a Mac OSX with Homebrew
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


brew update >/dev/null
echo ""
echo "Before my brew installs:"
brew list --versions

if [[ "$OIIOTARGET" == "clang-format" ]] ; then
    # If we are running for the sake of clang-format only, just install the
    # bare minimum packages and return.
    brew install ilmbase openexr llvm clang-format
    exit 0
fi

brew install --display-times gcc
brew link --overwrite gcc
brew install --display-times ccache cmake ninja
brew install --display-times ilmbase openexr
brew install --display-times opencolorio
brew install --display-times freetype
brew install --display-times libraw
brew install --display-times libpng webp jpeg-turbo
brew install --display-times openjpeg
brew install --display-times dcmtk
brew install --display-times qt
brew install --display-times -s field3d
# Note: field3d must be build from source to fix boost mismatch as of
# Nov 2018. Maybe it will be fixed soon? Check later.
brew install --display-times ffmpeg
brew install --display-times opencv
brew install --display-times tbb
brew install --display-times openvdb
brew install --display-times pybind11 numpy
if [[ "$LINKSTATIC" == "1" ]] ; then
    brew install --display-times little-cms2 tinyxml szip
    brew install --display-times homebrew/dupes/bzip2
    brew install --display-times yaml-cpp --with-static-lib
fi
if [[ "$CLANG_TIDY" != "" ]] ; then
    # If we are running for the sake of clang-tidy only, we will need
    # a modern clang version not just the xcode one.
    brew install llvm
fi

# Needed on some systems
pip install numpy

echo ""
echo "After brew installs:"
brew list --versions
