#!/usr/bin/env bash
#

set -ex

echo "Which g++ " `which g++`
g++ --version && true
ls /usr/bin/g++* && true
/usr/bin/g++ --version && true

export PATH=/opt/rh/devtoolset-6/root/usr/bin:/usr/local/bin:$PATH
#ls /opt/rh/devtoolset-6/root/usr/bin && true
#ls /usr/local/bin

ls /etc/yum.repos.d

sudo yum install -y giflib giflib-devel && true
sudo yum install -y opencv opencv-devel && true
sudo yum install -y Field3D Field3D-devel && true

#sudo rpm -v --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro && true
#sudo rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm && true
sudo yum install -y ffmpeg ffmpeg-devel && true



if [[ "$CXX" == "clang++" ]] ; then
    source src/build-scripts/build_llvm.bash
fi


src/build-scripts/install_test_images.bash

source src/build-scripts/build_pybind11.bash

if [[ "$OPENEXR_VERSION" != "" ]] ; then
    source src/build-scripts/build_openexr.bash
fi

if [[ "$LIBTIFF_VERSION" != "" ]] ; then
    source src/build-scripts/build_libtiff.bash
fi

if [[ "$OPENCOLORIO_VERSION" != "" ]] ; then
    # Temporary (?) fix: GH ninja having problems, fall back to make
    CMAKE_GENERATOR="Unix Makefiles" \
    source src/build-scripts/build_opencolorio.bash
fi
