#!/usr/bin/env bash

# Utility script to download and build openjph
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of openjph to download if we don't have it yet
OPENJPH_REPO=${OPENJPH_REPO:=https://github.com/aous72/OpenJPH.git}
OPENJPH_VERSION=${OPENJPH_VERSION:=0.28.1}

# Where to put openjph repo source (default to the ext area)
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OPENJPH_SRC_DIR=${OPENJPH_SRC_DIR:=${LOCAL_DEPS_DIR}/openjph}
OPENJPH_BUILD_DIR=${OPENJPH_BUILD_DIR:=${OPENJPH_SRC_DIR}/build}
OPENJPH_INSTALL_DIR=${OPENJPH_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
OPENJPH_BUILD_TYPE=${OPENJPH_BUILD_TYPE:=${CMAKE_BUILD_TYPE:-Release}}
#OPENJPH_CONFIG_OPTS=${OPENJPH_CONFIG_OPTS:=}

pwd
echo "openjph install dir will be: ${OPENJPH_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone openjph project from GitHub and build
if [[ ! -e ${OPENJPH_SRC_DIR} ]] ; then
    echo "git clone ${OPENJPH_REPO} ${OPENJPH_SRC_DIR}"
    git clone ${OPENJPH_REPO} ${OPENJPH_SRC_DIR}
fi
cd ${OPENJPH_SRC_DIR}

echo "git checkout ${OPENJPH_VERSION} --force"
git checkout ${OPENJPH_VERSION} --force

if [[ -z $DEP_DOWNLOAD_ONLY ]]; then
    time cmake -S . -B ${OPENJPH_BUILD_DIR} -DCMAKE_BUILD_TYPE=${OPENJPH_BUILD_TYPE} \
               -DCMAKE_INSTALL_PREFIX=${OPENJPH_INSTALL_DIR} \
               -DOJPH_BUILD_EXECUTABLES=OFF \
               -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
               -DBUILD_SHARED_LIBS=${OPENJPH_BUILD_SHARED_LIBS:-ON} \
               ${OPENJPH_CONFIG_OPTS}
    time cmake --build ${OPENJPH_BUILD_DIR} --target install
fi

# ls -R ${OPENJPH_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export openjph_ROOT=$OPENJPH_INSTALL_DIR
