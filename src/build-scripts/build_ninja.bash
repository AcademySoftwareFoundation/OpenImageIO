#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
DOWNLOADS_DIR=${DOWNLOADS_DIR:=${LOCAL_DEPS_DIR}/downloads}
NINJA_REPO=${NINJA_REPO:=https://github.com/ninja-build/ninja}
NINJA_VERSION=${NINJA_VERSION:=1.10.2}
NINJA_BRANCH=${NINJA_BRANCH:=v${NINJA_VERSION}}
NINJA_INSTALL_DIR=${NINJA_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist/bin}

mkdir -p "$DOWNLOADS_DIR"

# Check if the tar exist, download it if not
if [ ! -f $DOWNLOADS_DIR/ninja-v${NINJA_VERSION}.tar.gz ]; then
    curl --location ${NINJA_REPO}/archive/refs/tags/${NINJA_BRANCH}.tar.gz -o $DOWNLOADS_DIR/ninja-${NINJA_BRANCH}.tar.gz
fi

tar xf $DOWNLOADS_DIR/ninja-${NINJA_BRANCH}.tar.gz
pushd ninja-${NINJA_VERSION}

./configure.py --bootstrap

mkdir -p ${NINJA_INSTALL_DIR}
cp ninja ${NINJA_INSTALL_DIR}

popd

