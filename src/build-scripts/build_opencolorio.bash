#!/usr/bin/env bash

# Utility script to download and build OpenColorIO

# Exit the whole script if any command fails.
set -ex

# Which OCIO to retrieve, how to build it
OPENCOLORIO_REPO=${OPENCOLORIO_REPO:=https://github.com/AcademySoftwareFoundation/OpenColorIO.git}
OPENCOLORIO_VERSION=${OPENCOLORIO_VERSION:=v1.1.1}

# Where to install the final results
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OPENCOLORIO_SOURCE_DIR=${OPENCOLORIO_BUILD_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO}
OPENCOLORIO_BUILD_DIR=${OPENCOLORIO_BUILD_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO-build}
OPENCOLORIO_INSTALL_DIR=${OPENCOLORIO_INSTALL_DIR:=${LOCAL_DEPS_DIR}/dist}
OPENCOLORIO_CXX_FLAGS=${OPENCOLORIO_CXX_FLAGS:="-Wno-unused-function -Wno-deprecated-declarations -Wno-cast-qual -Wno-write-strings"}
# Just need libs:
OPENCOLORIO_BUILDOPTS="-DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF \
                       -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF \
                       -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_PYGLUE=OFF \
                       -DOCIO_BUILD_JAVA=OFF \
                       -DOCIO_BUILD_STATIC=${OCIO_BUILD_STATIC:=OFF}"
BASEDIR=`pwd`
pwd
echo "OpenColorIO install dir will be: ${OPENCOLORIO_INSTALL_DIR}"

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

# Clone OpenColorIO project from GitHub and build
if [[ ! -e OpenColorIO ]] ; then
    echo "git clone ${OPENCOLORIO_REPO} OpenColorIO"
    git clone ${OPENCOLORIO_REPO} OpenColorIO
fi
cd OpenColorIO

echo "git checkout ${OPENCOLORIO_VERSION} --force"
git checkout ${OPENCOLORIO_VERSION} --force
mkdir -p build
cd build
time cmake --config Release -DCMAKE_INSTALL_PREFIX=${OPENCOLORIO_INSTALL_DIR} -DCMAKE_CXX_FLAGS="${OPENCOLORIO_CXX_FLAGS}" ${OPENCOLORIO_BUILDOPTS} ..
time cmake --build . --config Release --target install
popd

ls -R ${OPENCOLORIO_INSTALL_DIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export OpenColorIO_ROOT=$OPENCOLORIO_INSTALL_DIR
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${OPENCOLORIO_INSTALL_DIR}/lib

