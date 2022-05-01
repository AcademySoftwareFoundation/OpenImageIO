#!/usr/bin/env bash
#

set -ex


#
# Install system packages when those are acceptable for dependencies.
#
if [[ "$ASWF_ORG" != ""  ]] ; then
    # Using ASWF CentOS container

    #ls /etc/yum.repos.d

    sudo yum install -y giflib giflib-devel && true
    sudo yum install -y opencv opencv-devel && true
    sudo yum install -y ffmpeg ffmpeg-devel && true
    if [[ "${EXTRA_DEP_PACKAGES}" != "" ]] ; then
        time sudo yum install -y ${EXTRA_DEP_PACKAGES}
    fi

    if [[ "${CONAN_LLVM_VERSION}" != "" ]] ; then
        mkdir conan
        pushd conan
        # Simple way to conan install just one package:
        #   conan install clang/${CONAN_LLVM_VERSION}@aswftesting/ci_common1 -g deploy -g virtualenv
        # But the below method can accommodate multiple requirements:
        echo "[imports]" >> conanfile.txt
        echo "., * -> ." >> conanfile.txt
        echo "[requires]" >> conanfile.txt
        echo "clang/${CONAN_LLVM_VERSION}@aswftesting/ci_common1" >> conanfile.txt
        time conan install .
        echo "--ls--"
        ls -R .
        export PATH=$PWD/bin:$PATH
        export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
        export LLVM_ROOT=$PWD
        popd
    fi

    if [[ "$CXX" == "icpc" || "$CC" == "icc" || "$USE_ICC" != "" || "$CXX" == "icpx" || "$CC" == "icx" || "$USE_ICX" != "" ]] ; then
        sudo cp src/build-scripts/oneAPI.repo /etc/yum.repos.d
        sudo yum install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic
        set +e; source /opt/intel/oneapi/setvars.sh; set -e
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

    if [[ "$CXX" == "icpc" || "$CC" == "icc" || "$USE_ICC" != "" || "$USE_ICX" != "" ]] ; then
        wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB
        sudo apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB
        echo "deb https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list
        time sudo apt-get update
        time sudo apt-get install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic
        set +e; source /opt/intel/oneapi/setvars.sh; set -e
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

if [[ "$OPENJPEG_VERSION" != "" ]] ; then
    source src/build-scripts/build_OpenJPEG.bash
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

if [[ "$USE_ICC" != "" ]] ; then
    # We used gcc for the prior dependency builds, but use icc for OIIO itself
    echo "which icpc:" $(which icpc)
    export CXX=icpc
    export CC=icc
fi

# Save the env for use by other stages
src/build-scripts/save-env.bash
