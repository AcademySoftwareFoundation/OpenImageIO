#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

# Utility script to download and build giflib

# Exit the whole script if any command fails.
set -ex

GIFLIB_REPO=${GIFLIB_REPO:=https://gitlab.com/giflib/giflib.git}
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
GIFLIB_BUILD_DIR=${GIFLIB_BUILD_DIR:=${LOCAL_DEPS_DIR}/giflib}
GIFLIB_INSTALL_DIR=${GIFLIB_INSTALL_DIR:=${PWD}/ext/dist}
GIFLIB_VERSION=${GIFLIB_VERSION:=5.2.1}
GIFLIB_CC=${GIFLIB_CC:=gcc}
BASEDIR=`pwd`
pwd
echo "giflib install dir will be: ${GIFLIB_INSTALL_DIR}"

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

# Clone giflib project and build
curl --location https://downloads.sourceforge.net/project/giflib/giflib-${GIFLIB_VERSION}.tar.gz -o giflib.tar.gz
tar xzf giflib.tar.gz
pushd giflib-${GIFLIB_VERSION}
cp Makefile Makefile.old
curl --location https://sourceforge.net/p/giflib/bugs/_discuss/thread/4e811ad29b/c323/attachment/Makefile.patch -o Makefile.patch
patch -p0 < Makefile.patch
popd

cd giflib-${GIFLIB_VERSION}
time make PREFIX=${GIFLIB_INSTALL_DIR} CC=${GIFLIB_CC} install

popd

# ls -R ${GIFLIB_INSTALL_DIR} && true

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export GIF_ROOT=$GIFLIB_INSTALL_DIR
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${GIFLIB_INSTALL_DIR}/lib
