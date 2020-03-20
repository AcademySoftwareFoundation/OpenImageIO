#!/bin/bash

# Utility script to download and build cmake

# Exit the whole script if any command fails.
set -ex

echo "Building cmake"
uname

CMAKE_VERSION=${CMAKE_VERSION:=3.12.4}
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
CMAKE_INSTALL_DIR=${CMAKE_INSTALL_DIR:=${LOCAL_DEPS_DIR}/cmake}

if [[ `uname` == "Linux" ]] ; then
    mkdir -p ${CMAKE_INSTALL_DIR} && true
    curl --location "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.sh" -o "cmake.sh"
    sh cmake.sh --skip-license --prefix=${CMAKE_INSTALL_DIR}
    export PATH=${CMAKE_INSTALL_DIR}/bin:$PATH
fi

