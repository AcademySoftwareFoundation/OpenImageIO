#!/usr/bin/env bash

# Utility script to download and build abi checking tools
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

# Where to install the final results
: ${LOCAL_DEPS_DIR:=${PWD}/ext}
: ${ABITOOLS_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

git clone https://github.com/lvc/vtable-dumper
pushd vtable-dumper ; make install prefix=${ABITOOLS_INSTALL_DIR} ; popd

git clone https://github.com/lvc/abi-dumper
pushd abi-dumper ; make install prefix=${ABITOOLS_INSTALL_DIR} ; popd

git clone https://github.com/lvc/abi-compliance-checker
pushd abi-compliance-checker ; make install prefix=${ABITOOLS_INSTALL_DIR} ; popd

popd

# ls -R ${LOCAL_DEPS_DIR}
export PATH=${PATH}:${ABITOOLS_INSTALL_DIR}/bin
