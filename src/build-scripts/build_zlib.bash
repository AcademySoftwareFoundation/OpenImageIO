#!/usr/bin/env bash

# Utility script to download and build zlib
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of zlib to download if we don't have it yet
ZLIB_REPO=${ZLIB_REPO:=https://github.com/madler/zlib.git}
ZLIB_VERSION=${ZLIB_VERSION:=v1.2.11}

# Where to put zlib repo source (default to the ext area)
ZLIB_SRC_DIR=${ZLIB_SRC_DIR:=${PWD}/ext/zlib}
# Temp build area (default to a build/ subdir under source)
ZLIB_BUILD_DIR=${ZLIB_BUILD_DIR:=${ZLIB_SRC_DIR}/build}
# Install area for zlib (default to ext/dist)
ZLIB_INSTALL_DIR=${ZLIB_INSTALL_DIR:=${PWD}/ext/dist}
#ZLIB_CONFIG_OPTS=${ZLIB_CONFIG_OPTS:=}

pwd
echo "zlib install dir will be: ${ZLIB_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone zlib project from GitHub and build
if [[ ! -e ${ZLIB_SRC_DIR} ]] ; then
    echo "git clone ${ZLIB_REPO} ${ZLIB_SRC_DIR}"
    git clone ${ZLIB_REPO} ${ZLIB_SRC_DIR}
fi
cd ${ZLIB_SRC_DIR}
echo "git checkout ${ZLIB_VERSION} --force"
git checkout ${ZLIB_VERSION} --force

mkdir -p ${ZLIB_BUILD_DIR} && true
cd ${ZLIB_BUILD_DIR}
time cmake --config Release \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${ZLIB_INSTALL_DIR} \
           ${ZLIB_CONFIG_OPTS} ..
time cmake --build . --config Release --target install

ls -R ${ZLIB_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export ZLIB_ROOT=$ZLIB_INSTALL_DIR

