#!/usr/bin/env bash
#

set -ex


#
# Install system packages when those are acceptable for dependencies.
#
if [[ "$ASWF_ORG" != ""  ]] ; then
    # Using ASWF CentOS container

    export PATH=/opt/rh/devtoolset-6/root/usr/bin:/usr/local/bin:$PATH

    #ls /etc/yum.repos.d

    sudo yum install -y giflib giflib-devel && true
    sudo yum install -y opencv opencv-devel && true
    sudo yum install -y Field3D Field3D-devel && true
    sudo yum install -y ffmpeg ffmpeg-devel && true
    if [[ "${EXTRA_DEP_PACKAGES}" != "" ]] ; then
        time sudo yum install -y ${EXTRA_DEP_PACKAGES}
    fi
else
    # Using native Ubuntu runner

    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    time sudo apt-get update

    time sudo apt-get -q install -y \
        git cmake ninja-build ccache g++ \
        libboost-dev libboost-thread-dev libboost-filesystem-dev \
        libilmbase-dev libopenexr-dev \
        libtbb-dev \
        libtiff-dev libgif-dev libpng-dev libraw-dev libwebp-dev \
        libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
        dcmtk libsquish-dev libopenvdb-dev \
        libfreetype6-dev \
        locales wget \
        libopencolorio-dev \
        libopencv-dev \
        qt5-default \
        libhdf5-dev
    if [[ "${EXTRA_DEP_PACKAGES}" != "" ]] ; then
        time sudo apt-get -q install -y ${EXTRA_DEP_PACKAGES}
    fi

    # Nonstandard python versions
    if [[ "${PYTHON_VERSION}" == "3.9" ]] ; then
        time sudo apt-get -q install -y python3.9-dev python3-numpy
        pip3 --version
        pip3 install numpy
    elif [[ "$PYTHON_VERSION" == "2.7" ]] ; then
        time sudo apt-get -q install -y python-dev python-numpy
    else
        pip3 install numpy
    fi

    if [[ "$USE_LIBHEIF" != "0" ]] ; then
       sudo add-apt-repository ppa:strukturag/libde265
       sudo add-apt-repository ppa:strukturag/libheif
       time sudo apt-get -q install -y libheif-dev
    fi

    export CMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu:$CMAKE_PREFIX_PATH

    if [[ "$CXX" == "g++-6" ]] ; then
        time sudo apt-get install -y g++-6
    elif [[ "$CXX" == "g++-7" ]] ; then
        time sudo apt-get install -y g++-7
    elif [[ "$CXX" == "g++-8" ]] ; then
        time sudo apt-get install -y g++-8
    elif [[ "$CXX" == "g++-9" ]] ; then
        time sudo apt-get install -y g++-9
    elif [[ "$CXX" == "g++-10" ]] ; then
        time sudo apt-get install -y g++-10
    elif [[ "$CXX" == "g++-11" ]] ; then
        time sudo apt-get install -y g++-11
    fi

fi

cmake --version


#
# If we're using clang to compile on native Ubuntu, we need to install it.
# If on an ASWF CentOS docker container, it already is installed.
#
if [[ "$CXX" == "clang++" && "$ASWF_ORG" == "" ]] ; then
    source src/build-scripts/build_llvm.bash
fi



#
# Packages we need to build from scratch.
#

source src/build-scripts/build_pybind11.bash

if [[ "$OPENEXR_VERSION" != "" ]] ; then
    source src/build-scripts/build_openexr.bash
fi

if [[ "$WEBP_VERSION" != "" ]] ; then
    source src/build-scripts/build_webp.bash
fi

if [[ "$LIBTIFF_VERSION" != "" ]] ; then
    source src/build-scripts/build_libtiff.bash
fi

if [[ "$LIBRAW_VERSION" != "" ]] ; then
    source src/build-scripts/build_libraw.bash
fi

if [[ "$PUGIXML_VERSION" != "" ]] ; then
    source src/build-scripts/build_pugixml.bash
    export MY_CMAKE_FLAGS+=" -DUSE_EXTERNAL_PUGIXML=1 "
fi

if [[ "$OPENCOLORIO_VERSION" != "" ]] ; then
    source src/build-scripts/build_opencolorio.bash
fi

if [[ "$PTEX_VERSION" != "" ]] ; then
    source src/build-scripts/build_Ptex.bash
fi

src/build-scripts/install_test_images.bash


# Save the env for use by other stages
src/build-scripts/save-env.bash
