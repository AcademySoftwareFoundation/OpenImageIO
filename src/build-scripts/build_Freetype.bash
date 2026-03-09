#!/usr/bin/env bash

# Utility script to download and build Freetype
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of Freetype to download if we don't have it yet
FREETYPE_REPO=${FREETYPE_REPO:=https://github.com/freetype/freetype.git}
FREETYPE_VERSION=${FREETYPE_VERSION:=VER-2-14-1}

# Where to put Freetype repo source (default to the ext area)
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
FREETYPE_SRC_DIR=${FREETYPE_SRC_DIR:=${LOCAL_DEPS_DIR}/Freetype}
FREETYPE_BUILD_DIR=${FREETYPE_BUILD_DIR:=${FREETYPE_SRC_DIR}/build}
FREETYPE_INSTALL_DIR=${FREETYPE_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
FREETYPE_BUILD_TYPE=${FREETYPE_BUILD_TYPE:=Release}

# Fix for freetype breaking against cmake 4.0 because of too-old cmake min.
# Remove when freetype is fixed to declare its own minimum high enough.
export CMAKE_POLICY_VERSION_MINIMUM=3.5

pwd
echo "Freetype install dir will be: ${FREETYPE_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone Freetype project from GitHub and build
if [[ ! -e ${FREETYPE_SRC_DIR} ]] ; then
    echo "git clone ${FREETYPE_REPO} ${FREETYPE_SRC_DIR}"
    git clone ${FREETYPE_REPO} ${FREETYPE_SRC_DIR}
fi
cd ${FREETYPE_SRC_DIR}

echo "git checkout ${FREETYPE_VERSION} --force"
git checkout ${FREETYPE_VERSION} --force

if [[ -z $DEP_DOWNLOAD_ONLY ]]; then
    time cmake -S . -B ${FREETYPE_BUILD_DIR} \
               -DCMAKE_BUILD_TYPE=${FREETYPE_BUILD_TYPE} \
               -DBUILD_SHARED_LIBS=${FREETYPE_BUILD_SHARED_LIBS:-ON} \
               -DCMAKE_INSTALL_PREFIX=${FREETYPE_INSTALL_DIR} \
               -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
               ${FREETYPE_CONFIG_OPTS}
    time cmake --build ${FREETYPE_BUILD_DIR} --config ${FREETYPE_BUILD_TYPE} --target install
fi

# ls -R ${FREETYPE_INSTALL_DIR}
popd


# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export Freetype_ROOT=$FREETYPE_INSTALL_DIR

