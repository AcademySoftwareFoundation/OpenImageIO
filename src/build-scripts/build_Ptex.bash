#!/usr/bin/env bash

# Utility script to download and build Ptex
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of ptex to download if we don't have it yet
: ${PTEX_REPO:=https://github.com/wdas/ptex.git}
: ${PTEX_VERSION:=v2.4.0}
: ${PTEX_SRC_DIR:=${PWD}/ext/ptex}
: ${PTEX_BUILD_DIR:=${PTEX_SRC_DIR}/build}
: ${PTEX_INSTALL_DIR:=${PWD}/ext/dist}

pwd
echo "Ptex install dir will be: ${PTEX_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone ptex project from GitHub and build
if [[ ! -e ${PTEX_SRC_DIR} ]] ; then
    echo "git clone ${PTEX_REPO} ${PTEX_SRC_DIR}"
    git clone ${PTEX_REPO} ${PTEX_SRC_DIR}
fi
cd ${PTEX_SRC_DIR}
echo "git checkout ${PTEX_VERSION} --force"
git checkout ${PTEX_VERSION} --force

time cmake -S . -B ${PTEX_BUILD_DIR} -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${PTEX_INSTALL_DIR} \
           ${PTEX_CONFIG_OPTS}
time cmake --build ${PTEX_BUILD_DIR} --config Release --target install

# ls -R ${PTEX_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export Ptex_ROOT=$PTEX_INSTALL_DIR
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PTEX_INSTALL_DIR}/lib:${PTEX_INSTALL_DIR}/lib64
