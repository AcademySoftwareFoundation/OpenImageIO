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

brew install gcc
brew link --overwrite gcc
brew install ccache cmake ninja
brew install ilmbase openexr
brew install opencolorio
brew install freetype
brew install libraw
brew install libpng webp jpeg-turbo
brew install openjpeg
brew install dcmtk
brew install qt
brew install -s field3d
# Note: field3d must be build from source to fix boost mismatch as of
# Nov 2018. Maybe it will be fixed soon? Check later.
brew install ffmpeg
brew install opencv
brew install tbb
brew install openvdb
brew install pybind11
if [[ "$LINKSTATIC" == "1" ]] ; then
    brew install little-cms2 tinyxml szip
    brew install homebrew/dupes/bzip2
    brew install yaml-cpp --with-static-lib
fi
if [[ "$CLANG_TIDY" != "" ]] ; then
    # If we are running for the sake of clang-tidy only, we will need
    # a modern clang version not just the xcode one.
    brew install llvm
fi

echo ""
echo "After brew installs:"
brew list --versions
