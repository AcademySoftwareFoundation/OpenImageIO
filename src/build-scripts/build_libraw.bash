#!/usr/bin/env bash

# Utility script to download and build LibRaw

# Exit the whole script if any command fails.
set -ex

# Which LibRaw to retrieve, how to build it
LIBRAW_REPO=${LIBRAW_REPO:=https://github.com/LibRaw/LibRaw.git}
LIBRAW_VERSION=${LIBRAW_VERSION:=0.19.5}

# Where to install the final results
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
LIBRAW_SOURCE_DIR=${LIBRAW_SOURCE_DIR:=${LOCAL_DEPS_DIR}/libraw}
LIBRAW_BUILD_DIR=${LIBRAW_BUILD_DIR:=${LOCAL_DEPS_DIR}/libraw-build}
LIBRAW_INSTALL_DIR=${LIBRAW_INSTALL_DIR:=${LOCAL_DEPS_DIR}/libraw-install}
LIBRAW_BUILD_TYPE=${LIBRAW_BUILD_TYPE:=Release}

pwd
echo "Building LibRaw ${LIBRAW_VERSION}"
echo "  build dir will be: ${LIBRAW_BUILD_DIR}"
echo "  install dir will be: ${LIBRAW_INSTALL_DIR}"
echo "  build type is ${LIBRAW_BUILD_TYPE}"


# Clone LibRaw project (including IlmBase) from GitHub and build
if [[ ! -e ${LIBRAW_SOURCE_DIR} ]] ; then
    echo "git clone ${LIBRAW_REPO} ${LIBRAW_SOURCE_DIR}"
    git clone ${LIBRAW_REPO} ${LIBRAW_SOURCE_DIR}
fi

mkdir -p ${LIBRAW_INSTALL_DIR} && true
mkdir -p ${LIBRAW_BUILD_DIR} && true

pushd ${LIBRAW_SOURCE_DIR}
git checkout ${LIBRAW_VERSION} --force

aclocal
autoreconf --install
./configure --prefix=${LIBRAW_INSTALL_DIR}
time make -j ${PARALLEL:=4} && make install

popd

#ls -R ${LIBRAW_INSTALL_DIR}

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export LibRaw_ROOT=$LIBRAW_INSTALL_DIR
export LibRaw_LIBRARY_DIR=$LIBRAW_INSTALL_DIR/lib
export LD_LIBRARY_PATH=$LIBRAW_ROOT/lib:$LD_LIBRARY_PATH

