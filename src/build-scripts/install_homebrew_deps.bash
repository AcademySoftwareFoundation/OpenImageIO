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

if [[ "$BUILDTARGET" == "clang-format" ]] ; then
    # If we are running for the sake of clang-format only, just install the
    # bare minimum packages and return.
    time brew install ilmbase openexr llvm
else
    # All cases except for clang-format target, we need the dependencies.
    time brew install gcc
    time brew link --overwrite gcc
    time brew install ccache cmake ninja
    time brew install ilmbase openexr
    time brew install opencolorio
    time brew install freetype
    time brew install libraw
    time brew install libpng webp jpeg-turbo
    time brew install openjpeg
    time brew install dcmtk
    time brew install qt
    time brew install -s field3d
    # Note: field3d must be build from source to fix boost mismatch as of
    # Nov 2018. Maybe it will be fixed soon? Check later.
    time brew install ffmpeg
    time brew install opencv
    time brew install tbb
    time brew install openvdb
    time brew install pybind11
    time brew install libheif
fi

if [[ "$LINKSTATIC" == "1" ]] ; then
    time brew install little-cms2 tinyxml szip
    time brew install homebrew/dupes/bzip2
    time brew install yaml-cpp --with-static-lib
fi
if [[ "$CLANG_TIDY" != "" ]] ; then
    # If we are running for the sake of clang-tidy only, we will need
    # a modern clang version not just the xcode one.
    time brew install llvm
fi

echo ""
echo "After brew installs:"
brew list --versions

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export PATH=/usr/local/opt/qt5/bin:$PATH ;
export PATH=/usr/local/opt/python/libexec/bin:$PATH ;
export PYTHONPATH=/usr/local/lib/python${PYTHON_VERSION}/site-packages:$PYTHONPATH ;
export PATH=/usr/local/Cellar/llvm/8.0.0/bin:$PATH ;

