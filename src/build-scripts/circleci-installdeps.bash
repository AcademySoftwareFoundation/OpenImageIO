#!/usr/bin/env bash
#

set -ex

#dpkg --list

time apt-get update

time apt-get install -y \
    git \
    cmake \
    ninja-build \
    g++ \
    ccache \
    libboost-dev libboost-thread-dev \
    libboost-filesystem-dev libboost-regex-dev \
    libtiff-dev \
    libilmbase-dev libopenexr-dev \
    python-dev python-numpy \
    libgif-dev \
    libpng-dev \
    libraw-dev \
    libwebp-dev \
    libfreetype6-dev \
    dcmtk \
    libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
    locales \
    opencolorio-tools \
    wget \
    libtbb-dev \
    libopenvdb-dev \
    libopencv-dev \
    ptex-base \
    dcmtk


# time apt-get install -y g++-4.8
#time apt-get install -y g++-7
#time apt-get install -y g++-8
# time apt-get install -y clang
# time apt-get install -y llvm
#time apt-get install -y libopenjpeg-dev
#time apt-get install -y libjpeg-turbo8-dev

dpkg --list

# I think opencolor-tools package is adequate for now. For more recent
# versions of OCIO, we could buidl it ourselves:
#CXX="ccache $CXX" src/build-scripts/build_ocio.bash
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/ext/OpenColorIO/dist/lib

src/build-scripts/install_test_images.bash
