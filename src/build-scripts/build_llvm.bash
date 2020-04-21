#!/usr/bin/env bash

# Utility script to download and build LLVM & clang

# Exit the whole script if any command fails.
set -ex

echo "Building LLVM"
uname


if [[ `uname` == "Linux" ]] ; then
    LLVM_VERSION=${LLVM_VERSION:=8.0.0}
    LLVM_INSTALL_DIR=${LLVM_INSTALL_DIR:=${PWD}/llvm-install}
    if [[ "$GITHUB_WORKFLOW" != "" ]] ; then
        LLVM_DISTRO_NAME=${LLVM_DISTRO_NAME:=ubuntu-18.04}
    elif [[ "$TRAVIS_DIST" == "trusty" ]] ; then
        LLVM_DISTRO_NAME=${LLVM_DISTRO_NAME:=ubuntu-14.04}
    elif [[ "$TRAVIS_DIST" == "xenial" ]] ; then
        LLVM_DISTRO_NAME=${LLVM_DISTRO_NAME:=ubuntu-16.04}
    elif [[ "$TRAVIS_DIST" == "bionic" ]] ; then
        LLVM_DISTRO_NAME=${LLVM_DISTRO_NAME:=ubuntu-18.04}
    else
        LLVM_DISTRO_NAME=${LLVM_DISTRO_NAME:=error}
    fi
    if [ "$LLVM_VERSION" == "5.0.0" ] ; then
        LLVMTAR=clang+llvm-${LLVM_VERSION}-linux-x86_64-${LLVM_DISTRO_NAME}.tar.xz
    else
        LLVMTAR=clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-${LLVM_DISTRO_NAME}.tar.xz
    fi
    echo LLVMTAR = $LLVMTAR
    curl --location http://releases.llvm.org/${LLVM_VERSION}/${LLVMTAR} -o $LLVMTAR
    ls -l $LLVMTAR
    tar xf $LLVMTAR
    rm -f $LLVMTAR
    echo "Installed ${LLVM_VERSION} in ${LLVM_INSTALL_DIR}"
    mkdir -p $LLVM_INSTALL_DIR && true
    mv clang+llvm*/* $LLVM_INSTALL_DIR
    export LLVM_DIRECTORY=$LLVM_INSTALL_DIR
    export PATH=${LLVM_INSTALL_DIR}/bin:$PATH
    ls -a $LLVM_DIRECTORY
fi
