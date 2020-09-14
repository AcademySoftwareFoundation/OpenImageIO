#!/usr/bin/env bash

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
    brew install --display-times ilmbase openexr llvm clang-format libtiff libpng boost ninja giflib
    brew install --display-times python pybind11 && true
    brew upgrade --display-times python && true
    brew link --overwrite python
    brew upgrade --display-times cmake && true
else
    # All cases except for clang-format target, we need the dependencies.
    brew install --display-times gcc ccache cmake ninja boost && true
    brew link --overwrite gcc
    brew install --display-times python && true
    brew upgrade --display-times python && true
    brew link --overwrite python
    brew upgrade --display-times cmake && true
    brew install --display-times libtiff ilmbase openexr opencolorio
    brew install --display-times libpng giflib webp jpeg-turbo openjpeg
    brew install --display-times freetype libraw dcmtk pybind11 numpy && true
    brew install --display-times ffmpeg libheif libsquish ptex && true
    brew install --display-times openvdb tbb && true
    brew install --display-times opencv && true
    brew install --display-times qt
    brew install --display-times field3d && true
fi

if [[ "$LINKSTATIC" == "1" ]] ; then
    brew install --display-times little-cms2 tinyxml szip
    brew install --display-times homebrew/dupes/bzip2
    brew install --display-times yaml-cpp --with-static-lib
fi
if [[ "$CLANG_TIDY" != "" ]] ; then
    # If we are running for the sake of clang-tidy only, we will need
    # a modern clang version not just the xcode one.
    brew install --display-times llvm
fi

echo ""
echo "After brew installs:"
brew list --versions

# Needed on some systems
pip install numpy

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export PATH=/usr/local/opt/qt5/bin:$PATH ;
export PATH=/usr/local/opt/python/libexec/bin:$PATH ;
export PYTHONPATH=/usr/local/lib/python${PYTHON_VERSION}/site-packages:$PYTHONPATH ;
export PATH=/usr/local/opt/llvm/bin:$PATH ;

# If field3d and hdf5 get even slightly out of sync, hdf5 will throw fits.
# This is unnecessary, so we disable the step to make CI more likely to
# pass in cases where they don't exactly match on the Travis/GH instances.
export HDF5_DISABLE_VERSION_CHECK=1
