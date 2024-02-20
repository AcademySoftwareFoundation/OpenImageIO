#!/usr/bin/env bash

# Utility script to download and build OpenJPEG
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of OpenJPEG to download if we don't have it yet
OPENJPEG_REPO=${OPENJPEG_REPO:=https://github.com/uclouvain/openjpeg.git}
OPENJPEG_VERSION=${OPENJPEG_VERSION:=v2.4.0}

# Where to put OpenJPEG repo source (default to the ext area)
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OPENJPEG_SRC_DIR=${OPENJPEG_SRC_DIR:=${LOCAL_DEPS_DIR}/OpenJPEG}
OPENJPEG_BUILD_DIR=${OPENJPEG_BUILD_DIR:=${OPENJPEG_SRC_DIR}/build}
OPENJPEG_INSTALL_DIR=${OPENJPEG_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
#OPENJPEG_CONFIG_OPTS=${OPENJPEG_CONFIG_OPTS:=}

pwd
echo "OpenJPEG install dir will be: ${OPENJPEG_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone OpenJPEG project from GitHub and build
if [[ ! -e ${OPENJPEG_SRC_DIR} ]] ; then
    echo "git clone ${OPENJPEG_REPO} ${OPENJPEG_SRC_DIR}"
    git clone ${OPENJPEG_REPO} ${OPENJPEG_SRC_DIR}
fi
cd ${OPENJPEG_SRC_DIR}

echo "git checkout ${OPENJPEG_VERSION} --force"
git checkout ${OPENJPEG_VERSION} --force

if [[ -z $DEP_DOWNLOAD_ONLY ]]; then
    time cmake -S . -B ${OPENJPEG_BUILD_DIR} \
               -DCMAKE_BUILD_TYPE=Release \
               -DCMAKE_INSTALL_PREFIX=${OPENJPEG_INSTALL_DIR} \
               -DBUILD_CODEC=OFF \
               ${OPENJPEG_CONFIG_OPTS}
    time cmake --build ${OPENJPEG_BUILD_DIR} --config Release --target install
fi

# ls -R ${OPENJPEG_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export OpenJPEG_ROOT=$OPENJPEG_INSTALL_DIR

