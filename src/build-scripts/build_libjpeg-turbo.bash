#!/usr/bin/env bash

# Utility script to download and build libjpeg-turbo
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of libjpeg-turbo to download if we don't have it yet
LIBJPEGTURBO_REPO=${LIBJPEGTURBO_REPO:=https://github.com/libjpeg-turbo/libjpeg-turbo.git}
LIBJPEGTURBO_VERSION=${LIBJPEGTURBO_VERSION:=2.0.5}

# Where to put libjpeg-turbo repo source (default to the ext area)
LIBJPEGTURBO_SRC_DIR=${LIBJPEGTURBO_SRC_DIR:=${PWD}/ext/libjpeg-turbo}
# Temp build area (default to a build/ subdir under source)
LIBJPEGTURBO_BUILD_DIR=${LIBJPEGTURBO_BUILD_DIR:=${LIBJPEGTURBO_SRC_DIR}/build}
# Install area for libjpeg-turbo (default to ext/dist)
LIBJPEGTURBO_INSTALL_DIR=${LIBJPEGTURBO_INSTALL_DIR:=${PWD}/ext/dist}
#LIBJPEGTURBO_CONFIG_OPTS=${LIBJPEGTURBO_CONFIG_OPTS:=}

pwd
echo "libjpeg-turbo install dir will be: ${LIBJPEGTURBO_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone libjpeg-turbo project from GitHub and build
if [[ ! -e ${LIBJPEGTURBO_SRC_DIR} ]] ; then
    echo "git clone ${LIBJPEGTURBO_REPO} ${LIBJPEGTURBO_SRC_DIR}"
    git clone ${LIBJPEGTURBO_REPO} ${LIBJPEGTURBO_SRC_DIR}
fi
cd ${LIBJPEGTURBO_SRC_DIR}
echo "git checkout ${LIBJPEGTURBO_VERSION} --force"
git checkout ${LIBJPEGTURBO_VERSION} --force

mkdir -p ${LIBJPEGTURBO_BUILD_DIR}
cd ${LIBJPEGTURBO_BUILD_DIR}
time cmake --config Release \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${LIBJPEGTURBO_INSTALL_DIR} \
           ${LIBJPEGTURBO_CONFIG_OPTS} ..
time cmake --build . --config Release --target install

ls -R ${LIBJPEGTURBO_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export JPEGTurbo_ROOT=$LIBJPEGTURBO_INSTALL_DIR

