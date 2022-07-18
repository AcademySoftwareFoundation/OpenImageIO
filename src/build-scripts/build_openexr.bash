#!/usr/bin/env bash

# Utility script to download and build OpenEXR & Imath

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Which OpenEXR to retrieve, how to build it
OPENEXR_REPO=${OPENEXR_REPO:=https://github.com/AcademySoftwareFoundation/openexr.git}
OPENEXR_VERSION=${OPENEXR_VERSION:=v3.2.4}

# Where to install the final results
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OPENEXR_SOURCE_DIR=${OPENEXR_SOURCE_DIR:=${LOCAL_DEPS_DIR}/openexr}
OPENEXR_BUILD_DIR=${OPENEXR_BUILD_DIR:=${LOCAL_DEPS_DIR}/openexr-build}
OPENEXR_INSTALL_DIR=${OPENEXR_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
OPENEXR_BUILD_TYPE=${OPENEXR_BUILD_TYPE:=Release}
OPENEXR_CMAKE_FLAGS=${OPENEXR_CMAKE_FLAGS:=""}
OPENEXR_CXX_FLAGS=${OPENEXR_CXX_FLAGS:=""}
OPENEXR_BUILD_SHARED_LIBS=${OPENEXR_BUILD_SHARED_LIBS:="ON"}
BASEDIR=$PWD

if [[ "$OSTYPE" == "msys" ]]; then
    OPENEXR_CXX_FLAGS="${OPENEXR_CXX_FLAGS} /W1 /EHsc /DWIN32=1"
    if [[ "${OPENEXR_BUILD_SHARED_LIBS}" != "ON" ]]; then
        OPENEXR_CXX_FLAGS="${OPENEXR_CXX_FLAGS} /MT"
    fi
fi

pwd
echo "Building OpenEXR ${OPENEXR_VERSION}"
echo "OpenEXR build dir will be: ${OPENEXR_BUILD_DIR}"
echo "OpenEXR install dir will be: ${OPENEXR_INSTALL_DIR}"
echo "OpenEXR Build type is ${OPENEXR_BUILD_TYPE}"

# Clone OpenEXR project (including Imath) from GitHub and build
if [[ ! -e ${OPENEXR_SOURCE_DIR} ]] ; then
    echo "git clone ${OPENEXR_REPO} ${OPENEXR_SOURCE_DIR}"
    git clone ${OPENEXR_REPO} ${OPENEXR_SOURCE_DIR}
fi
mkdir -p ${OPENEXR_INSTALL_DIR} && true

pushd ${OPENEXR_SOURCE_DIR}
git checkout ${OPENEXR_VERSION} --force

# Simplified setup for 2.4+
cd ${OPENEXR_BUILD_DIR}
cmake -DCMAKE_BUILD_TYPE=${OPENEXR_BUILD_TYPE} \
        -DBUILD_SHARED_LIBS=${OPENEXR_BUILD_SHARED_LIBS} \
        -DCMAKE_INSTALL_PREFIX="${OPENEXR_INSTALL_DIR}" \
        -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
        -DOPENEXR_BUILD_UTILS=0 \
        -DBUILD_TESTING=0 \
        -DOPENEXR_VIEWERS_ENABLE=0 \
        -DINSTALL_OPENEXR_EXAMPLES=0 \
        -DOPENEXR_INSTALL_EXAMPLES=0 \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_CXX_FLAGS="${OPENEXR_CXX_FLAGS}" \
        -DCMAKE_CXX_FLAGS_RELEASE="${OPENEXR_CXX_FLAGS}" \
        -DCMAKE_C_FLAGS="${OPENEXR_CXX_FLAGS}" \
        -DCMAKE_C_FLAGS_RELEASE="${OPENEXR_CXX_FLAGS}" \
        -DCMAKE_MODULE_PATH=${CONAN_CMAKE_FILES} \
        ${OPENEXR_CMAKE_FLAGS} ${OPENEXR_SOURCE_DIR}
time cmake --build . --target install --config ${OPENEXR_BUILD_TYPE}

popd

#ls -R ${OPENEXR_INSTALL_DIR}

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export OPENEXR_ROOT=$OPENEXR_INSTALL_DIR
export OPENEXR_LIBRARY_DIR=$OPENEXR_INSTALL_DIR/lib
export LD_LIBRARY_PATH=$OPENEXR_ROOT/lib:$LD_LIBRARY_PATH

