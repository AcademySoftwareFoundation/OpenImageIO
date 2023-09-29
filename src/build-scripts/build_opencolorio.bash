#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Utility script to download and build OpenColorIO

# Exit the whole script if any command fails.
set -ex

# Which OCIO to retrieve, how to build it
OPENCOLORIO_REPO=${OPENCOLORIO_REPO:=https://github.com/AcademySoftwareFoundation/OpenColorIO.git}
OPENCOLORIO_VERSION=${OPENCOLORIO_VERSION:=v2.2.1}

# Where to install the final results
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OPENCOLORIO_SOURCE_DIR=${OPENCOLORIO_SOURCE_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO}
OPENCOLORIO_BUILD_DIR=${OPENCOLORIO_BUILD_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO-build}
OPENCOLORIO_INSTALL_DIR=${OPENCOLORIO_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
OPENCOLORIO_BUILD_TYPE=${OPENCOLORIO_BUILD_TYPE:=Release}
OPENCOLORIO_CXX_FLAGS=${OPENCOLORIO_CXX_FLAGS:="-Wno-unused-function -Wno-deprecated-declarations -Wno-cast-qual -Wno-write-strings"}
# Just need libs:
OPENCOLORIO_BUILDOPTS="-DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF \
                       -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF \
                       -DOCIO_BUILD_GPU_TESTS=OFF \
                       -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_PYGLUE=OFF \
                       -DOCIO_BUILD_JAVA=OFF \
                       -DBUILD_SHARED_LIBS=${OPENCOLORIO_BUILD_SHARED_LIBS:=ON}"
BASEDIR=`pwd`
pwd
echo "OpenColorIO install dir will be: ${OPENCOLORIO_INSTALL_DIR}"

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

# Clone OpenColorIO project from GitHub and build
if [[ ! -e ${OPENCOLORIO_SOURCE_DIR} ]] ; then
    echo "git clone ${OPENCOLORIO_REPO} ${OPENCOLORIO_SOURCE_DIR}"
    git clone ${OPENCOLORIO_REPO} ${OPENCOLORIO_SOURCE_DIR}
fi
cd ${OPENCOLORIO_SOURCE_DIR}

echo "git checkout ${OPENCOLORIO_VERSION} --force"
git checkout ${OPENCOLORIO_VERSION} --force
time cmake -S . -B ${OPENCOLORIO_BUILD_DIR} \
           -DCMAKE_BUILD_TYPE=${OPENCOLORIO_BUILD_TYPE} \
           -DCMAKE_INSTALL_PREFIX=${OPENCOLORIO_INSTALL_DIR} \
           -DCMAKE_CXX_FLAGS="${OPENCOLORIO_CXX_FLAGS}" \
           ${OPENCOLORIO_BUILDOPTS}
time cmake --build ${OPENCOLORIO_BUILD_DIR} --config Release --target install
popd

# ls -R ${OPENCOLORIO_INSTALL_DIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export OpenColorIO_ROOT=$OPENCOLORIO_INSTALL_DIR
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${OPENCOLORIO_INSTALL_DIR}/lib

