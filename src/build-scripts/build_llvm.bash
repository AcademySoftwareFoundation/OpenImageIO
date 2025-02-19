#!/usr/bin/env bash

# Utility script to download and build LLVM & clang
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exit the whole script if any command fails.
set -ex

echo "Building LLVM"
uname

: ${LLVM_VERSION:=18.1.8}
: ${LLVM_INSTALL_DIR:=${PWD}/llvm-install}
mkdir -p $LLVM_INSTALL_DIR || true

if [[ `uname` == "Linux" ]] ; then
    : ${LLVM_DISTRO_NAME:=ubuntu-18.04}
    LLVMTAR=clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-${LLVM_DISTRO_NAME}.tar.xz
    echo LLVMTAR = $LLVMTAR
    curl --location https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/${LLVMTAR} -o $LLVMTAR
    ls -l $LLVMTAR
    tar xf $LLVMTAR
    rm -f $LLVMTAR
    mv clang+llvm*/* $LLVM_INSTALL_DIR
elif [[ `uname -s` == "Windows" || "${RUNNER_OS}" == "Windows" ]] ; then
    echo "Installing Windows LLVM"
    : ${LLVM_DISTRO_NAME:=ubuntu-18.04}
    LLVMTAR=clang+llvm-${LLVM_VERSION}-x86_64-pc-windows-msvc.tar.xz
    echo LLVMTAR = $LLVMTAR
    curl --location https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/${LLVMTAR} -o $LLVMTAR
    ls -l $LLVMTAR
    tar xf $LLVMTAR
    rm -f $LLVMTAR
    mv clang+llvm*/* $LLVM_INSTALL_DIR
else
    echo Bad uname `uname`
fi

echo "Installed LLVM ${LLVM_VERSION} in ${LLVM_INSTALL_DIR}"
ls -a $LLVM_INSTALL_DIR || true
ls -a $LLVM_INSTALL_DIR/* || true
export LLVM_DIRECTORY=$LLVM_INSTALL_DIR
export PATH=${LLVM_INSTALL_DIR}/bin:$PATH
export LLVM_ROOT=${LLVM_INSTALL_DIR}
