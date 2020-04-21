#!/usr/bin/env bash

# Utility script to download and build OpenColorIO

# Exit the whole script if any command fails.
set -ex

OCIO_REPO=${OCIO_REPO:=https://github.com/AcademySoftwareFoundation/OpenColorIO.git}
LOCAL_DEPS_DIR=${LOCAL_DEPS_DIR:=${PWD}/ext}
OCIO_BUILD_DIR=${OCIO_BUILD_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO}
OCIO_INSTALL_DIR=${OCIO_INSTALL_DIR:=${LOCAL_DEPS_DIR}/OpenColorIO/dist}
OCIO_VERSION=${OCIO_VERSION:=1.1.1}
OCIO_BRANCH=${OCIO_BRANCH:=v${OCIO_VERSION}}
OCIO_CXX_FLAGS=${OCIO_CXX_FLAGS:="-Wno-unused-function -Wno-deprecated-declarations -Wno-cast-qual -Wno-write-strings"}
# Just need libs:
OCIO_BUILDOPTS="-DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF \
               -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF \
               -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_PYGLUE=OFF \
               -DOCIO_BUILD_JAVA=OFF \
               -DOCIO_BUILD_STATIC=${OCIO_BUILD_STATIC:=OFF}"
BASEDIR=`pwd`
pwd
echo "OpenColorIO install dir will be: ${OCIO_INSTALL_DIR}"

mkdir -p ${LOCAL_DEPS_DIR}
pushd ${LOCAL_DEPS_DIR}

# Clone OpenColorIO project from GitHub and build
if [[ ! -e OpenColorIO ]] ; then
    echo "git clone ${OCIO_REPO} OpenColorIO"
    git clone ${OCIO_REPO} OpenColorIO
fi
cd OpenColorIO

echo "git checkout ${OCIO_BRANCH} --force"
git checkout ${OCIO_BRANCH} --force
mkdir -p build
cd build
time cmake --config Release -DCMAKE_INSTALL_PREFIX=${OCIO_INSTALL_DIR} -DCMAKE_CXX_FLAGS="${OCIO_CXX_FLAGS}" ${OCIO_BUILDOPTS} ..
time cmake --build . --config Release --target install
popd

ls -R ${OCIO_INSTALL_DIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export OpenColorIO_ROOT=$OCIO_INSTALL_DIR
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${OCIO_INSTALL_DIR}/lib

