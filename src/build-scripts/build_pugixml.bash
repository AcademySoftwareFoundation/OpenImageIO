#!/usr/bin/env bash

# Utility script to download and build pugixml
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# Exit the whole script if any command fails.
set -ex

# Repo and branch/tag/commit of pugixml to download if we don't have it yet
PUGIXML_REPO=${PUGIXML_REPO:=https://github.com/zeux/pugixml.git}
PUGIXML_VERSION=${PUGIXML_VERSION:=1.10}
PUGIXML_BRANCH=${PUGIXML_BRANCH:=v${PUGIXML_VERSION}}

# Where to put pugixml repo source (default to the ext area)
PUGIXML_SRC_DIR=${PUGIXML_SRC_DIR:=${PWD}/ext/pugixml}
# Temp build area (default to a build/ subdir under source)
PUGIXML_BUILD_DIR=${PUGIXML_BUILD_DIR:=${PUGIXML_SRC_DIR}/build}
# Install area for pugixml (default to ext/dist)
PUGIXML_INSTALL_DIR=${PUGIXML_INSTALL_DIR:=${PWD}/ext/dist}
#PUGIXML_BUILD_OPTS=${PUGIXML_BUILD_OPTS:=}

pwd
echo "pugixml install dir will be: ${PUGIXML_INSTALL_DIR}"

mkdir -p ./ext
pushd ./ext

# Clone pugixml project from GitHub and build
if [[ ! -e ${PUGIXML_SRC_DIR} ]] ; then
    echo "git clone ${PUGIXML_REPO} ${PUGIXML_SRC_DIR}"
    git clone ${PUGIXML_REPO} ${PUGIXML_SRC_DIR}
fi
cd ${PUGIXML_SRC_DIR}
echo "git checkout ${PUGIXML_BRANCH} --force"
git checkout ${PUGIXML_BRANCH} --force

mkdir -p ${PUGIXML_BUILD_DIR}
cd ${PUGIXML_BUILD_DIR}
time cmake --config Release \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=${PUGIXML_INSTALL_DIR} \
           -DBUILD_TESTS=OFF \
           ${PUGIXML_BUILD_OPTS} ..
time cmake --build . --config Release --target install

ls -R ${PUGIXML_INSTALL_DIR}
popd

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export pugixml_ROOT=$PUGIXML_INSTALL_DIR

