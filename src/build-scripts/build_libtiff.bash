#!/usr/bin/env bash

# Utility script to download and build libtiff

# Exit the whole script if any command fails.
set -ex

LIBTIFF_REPO=${LIBTIFF_REPO:=https://gitlab.com/libtiff/libtiff.git}
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
LIBTIFF_BUILD_DIR=${LIBTIFF_BUILD_DIR:=${LOCAL_DEPS_DIR}/libtiff}
LIBTIFF_INSTALL_DIR=${LIBTIFF_INSTALL_DIR:=${PWD}/ext/dist}
LIBTIFF_VERSION=${LIBTIFF_VERSION:=v4.1.0}
if [[ `uname` == `Linux` ]] ; then
    LIBTIFF_CXX_FLAGS=${LIBTIFF_CXX_FLAGS:="-O3 -Wno-unused-function -Wno-deprecated-declarations -Wno-cast-qual -Wno-write-strings"}
fi
LIBTIFF_BUILDOPTS="${LIBTIFF_BUILDOPTS}"
BASEDIR=`pwd`
pwd
echo "libtiff install dir will be: ${LIBTIFF_INSTALL_DIR}"

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

# Clone libtiff project from GitHub and build
if [[ ! -e libtiff ]] ; then
    echo "git clone ${LIBTIFF_REPO} libtiff"
    git clone ${LIBTIFF_REPO} libtiff
fi
cd libtiff

echo "git checkout ${LIBTIFF_VERSION} --force"
git checkout ${LIBTIFF_VERSION} --force
mkdir -p build
cd build
time cmake --config Release \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${LIBTIFF_INSTALL_DIR} \
           -DCMAKE_CXX_FLAGS="${LIBTIFF_CXX_FLAGS}" \
           ${LIBTIFF_BUILDOPTS} ..
time cmake --build . --config Release --target install
popd

ls -R ${LIBTIFF_INSTALL_DIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export TIFF_ROOT=$LIBTIFF_INSTALL_DIR
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${LIBTIFF_INSTALL_DIR}/lib

