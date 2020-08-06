#!/usr/bin/env bash

# Utility script to download and build pybind11

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of pybind11 to download if we don't have it yet
PYBIND11_REPO=${PYBIND11_REPO:=https://github.com/pybind/pybind11.git}
PYBIND11_VERSION=${PYBIND11_VERSION:=v2.4.3}

# Where to put pybind11 repo source (default to the ext area)
PYBIND11_SRC_DIR=${PYBIND11_SRC_DIR:=${PWD}/ext/pybind11}
# Temp build area (default to a build/ subdir under source)
PYBIND11_BUILD_DIR=${PYBIND11_BUILD_DIR:=${PYBIND11_SRC_DIR}/build}
# Install area for pybind11 (default to ext/dist)
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
PYBIND11_INSTALL_DIR=${PYBIND11_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
#PYBIND11_BUILD_OPTS=${PYBIND11_BUILD_OPTS:=}

if [[ "${PYTHON_VERSION}" != "" ]] ; then
    PYBIND11_BUILD_OPTS+=" -DPYBIND11_PYTHON_VERSION=${PYTHON_VERSION}"
fi

pwd
echo "pybind11 install dir will be: ${PYBIND11_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone pybind11 project from GitHub and build
if [[ ! -e ${PYBIND11_SRC_DIR} ]] ; then
    echo "git clone ${PYBIND11_REPO} ${PYBIND11_SRC_DIR}"
    git clone ${PYBIND11_REPO} ${PYBIND11_SRC_DIR}
fi
cd ${PYBIND11_SRC_DIR}
echo "git checkout ${PYBIND11_VERSION} --force"
git checkout ${PYBIND11_VERSION} --force

mkdir -p ${PYBIND11_BUILD_DIR}
cd ${PYBIND11_BUILD_DIR}
time cmake --config Release \
           -DCMAKE_INSTALL_PREFIX=${PYBIND11_INSTALL_DIR} \
           -DPYBIND11_TEST=OFF \
           ${PYBIND11_BUILD_OPTS} ..
time cmake --build . --config Release --target install

ls -R ${PYBIND11_INSTALL_DIR}
popd

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export pybind11_ROOT=$PYBIND11_INSTALL_DIR

