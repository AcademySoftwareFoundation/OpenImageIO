#!/usr/bin/env bash

# Utility script to download and build webp
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of webp to download if we don't have it yet
WEBP_REPO=${WEBP_REPO:=https://github.com/webmproject/libwebp.git}
WEBP_VERSION=${WEBP_VERSION:=v1.1.0}

# Where to put webp repo source (default to the ext area)
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
WEBP_SRC_DIR=${WEBP_SRC_DIR:=${LOCAL_DEPS_DIR}/webp}
WEBP_BUILD_DIR=${WEBP_BUILD_DIR:=${WEBP_SRC_DIR}/build}
WEBP_INSTALL_DIR=${WEBP_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
WEBP_BUILD_TYPE=${WEBP_BUILD_TYPE:=${CMAKE_BUILD_TYPE:-Release}}
#WEBP_CONFIG_OPTS=${WEBP_CONFIG_OPTS:=}

pwd
echo "webp install dir will be: ${WEBP_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone webp project from GitHub and build
if [[ ! -e ${WEBP_SRC_DIR} ]] ; then
    echo "git clone ${WEBP_REPO} ${WEBP_SRC_DIR}"
    git clone ${WEBP_REPO} ${WEBP_SRC_DIR}
fi
cd ${WEBP_SRC_DIR}

echo "git checkout ${WEBP_VERSION} --force"
git checkout ${WEBP_VERSION} --force

if [[ -z $DEP_DOWNLOAD_ONLY ]]; then
    time cmake -S . -B ${WEBP_BUILD_DIR} -DCMAKE_BUILD_TYPE=${WEBP_BUILD_TYPE} \
               -DCMAKE_INSTALL_PREFIX=${WEBP_INSTALL_DIR} \
               -DWEBP_BUILD_ANIM_UTILS=OFF \
               -DWEBP_BUILD_CWEBP=OFF \
               -DWEBP_BUILD_DWEBP=OFF \
               -DWEBP_BUILD_VWEBP=OFF \
               -DWEBP_BUILD_GIF2WEBPx=OFF \
               -DWEBP_BUILD_IMG2WEBP=OFF \
               -DWEBP_BUILD_EXTRAS=OFF \
               -DBUILD_SHARED_LIBS=ON \
               ${WEBP_CONFIG_OPTS}
    time cmake --build ${WEBP_BUILD_DIR} --target install
fi


# ls -R ${WEBP_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export WebP_ROOT=$WEBP_INSTALL_DIR

