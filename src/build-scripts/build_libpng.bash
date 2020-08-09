#!/usr/bin/env bash

# Utility script to download and build libpng
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of libpng to download if we don't have it yet
LIBPNG_REPO=${LIBPNG_REPO:=https://github.com/glennrp/libpng.git}
LIBPNG_VERSION=${LIBPNG_VERSION:=v1.6.35}

# Where to put libpng repo source (default to the ext area)
LIBPNG_SRC_DIR=${LIBPNG_SRC_DIR:=${PWD}/ext/libpng}
# Temp build area (default to a build/ subdir under source)
LIBPNG_BUILD_DIR=${LIBPNG_BUILD_DIR:=${LIBPNG_SRC_DIR}/build}
# Install area for libpng (default to ext/dist)
LIBPNG_INSTALL_DIR=${LIBPNG_INSTALL_DIR:=${PWD}/ext/dist}
#LIBPNG_CONFIG_OPTS=${LIBPNG_CONFIG_OPTS:=}

pwd
echo "libpng install dir will be: ${LIBPNG_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone libpng project from GitHub and build
if [[ ! -e ${LIBPNG_SRC_DIR} ]] ; then
    echo "git clone ${LIBPNG_REPO} ${LIBPNG_SRC_DIR}"
    git clone ${LIBPNG_REPO} ${LIBPNG_SRC_DIR}
fi
cd ${LIBPNG_SRC_DIR}
echo "git checkout ${LIBPNG_VERSION} --force"
git checkout ${LIBPNG_VERSION} --force

mkdir -p ${LIBPNG_BUILD_DIR}
cd ${LIBPNG_BUILD_DIR}
time cmake --config Release \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${LIBPNG_INSTALL_DIR} \
           -DPNG_EXECUTABLES=OFF \
           -DPNG_TESTS=OFF \
           ${LIBPNG_CONFIG_OPTS} ..
time cmake --build . --config Release --target install

ls -R ${LIBPNG_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export PNG_ROOT=$LIBPNG_INSTALL_DIR

