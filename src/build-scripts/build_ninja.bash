#!/usr/bin/env bash

# Exit the whole script if any command fails.
set -ex

LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
DOWNLOADS_DIR=${DOWNLOADS_DIR:=${LOCAL_DEPS_DIR}/downloads}
NINJA_REPO=${NINJA_REPO:=https://github.com/ninja-build/ninja}
NINJA_VERSION=${NINJA_VERSION:=1.10.2}
NINJA_BRANCH=${NINJA_BRANCH:=v${NINJA_VERSION}}
NINJA_INSTALL_DIR=${NINJA_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist/bin}

if [ ! -f $DOWNLOADS_DIR/ninja-${NINJA_VERSION}.zip ]; then
    curl --location ${NINJA_REPO}/archive/${NINJA_BRANCH}.tar.gz -o $DOWNLOADS_DIR/ninja-${NINJA_BRANCH}.tar.gz
fi

tar xf $DOWNLOADS_DIR/ninja-${NINJA_BRANCH}.tar.gz
pushd ninja-${NINJA_VERSION}

./configure.py --bootstrap

mkdir -p ${NINJA_INSTALL_DIR}
cp ninja ${NINJA_INSTALL_DIR}

popd

