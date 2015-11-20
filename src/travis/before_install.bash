#!/bin/bash


if [ $TRAVIS_OS_NAME == osx ] ; then
    brew update >/dev/null
    echo ""
    echo "Before my brew installs:"
    brew list --versions
    brew install ilmbase openexr
    brew install boost-python
    brew install opencolorio
    brew install freetype libraw
#    brew install homebrew/science/hdf5 --with-threadsafe
#    brew install field3d webp ffmpeg openjpeg opencv
    echo ""
    echo "After brew installs:"
    brew list --versions
fi

if [ $TRAVIS_OS_NAME == linux ] ; then
    apt-cache pkgnames
fi
