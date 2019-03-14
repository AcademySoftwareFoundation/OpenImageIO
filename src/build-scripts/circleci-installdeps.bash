#!/usr/bin/env bash
#

set -ex

#dpkg --list

time apt-get update

time apt-get install -y git
time apt-get install -y cmake
time apt-get install -y g++
# time apt-get install -y g++-4.8
#time apt-get install -y g++-7
#time apt-get install -y g++-8
time apt-get install -y ccache
# time apt-get install -y clang
# time apt-get install -y llvm
time apt-get install -y libboost-dev
time apt-get install -y libboost-thread-dev
time apt-get install -y libboost-filesystem-dev
time apt-get install -y libboost-regex-dev
time apt-get install -y libtiff-dev
time apt-get install -y libilmbase-dev
time apt-get install -y libopenexr-dev
time apt-get install -y python-dev
time apt-get install -y libboost-python-dev
time apt-get install -y python-numpy
time apt-get install -y libgif-dev
time apt-get install -y libpng-dev
#time apt-get install -y libopenjpeg-dev
time apt-get install -y libraw-dev
time apt-get install -y libwebp-dev
time apt-get install -y libfreetype6-dev
#time apt-get install -y libjpeg-turbo8-dev
time apt-get install -y dcmtk
time apt-get install -y libavcodec-dev
time apt-get install -y libavformat-dev
time apt-get install -y libswscale-dev
time apt-get install -y libavutil-dev
time apt-get install -y locales
time apt-get install -y opencolorio-tools
time apt-get install -y ninja-build

dpkg --list

# I think opencolor-tools package is adequate for now. For more recent
# versions of OCIO, we could buidl it ourselves:
#CXX="ccache $CXX" src/build-scripts/build_ocio.bash
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/ext/OpenColorIO/dist/lib

src/build-scripts/install_test_images.bash
