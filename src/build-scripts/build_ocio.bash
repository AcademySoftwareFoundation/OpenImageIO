#!/bin/bash

OCIOREPO=${OCIOREPO:=https://github.com/imageworks/OpenColorIO.git}
OCIOBUILDDIR=${OCIOBUILDDIR:=${PWD}/ext/OpenColorIO}
OCIOINSTALLDIR=${OCIOINSTALLDIR:=${PWD}/ext/OpenColorIO/dist}
OCIOBRANCH=${OCIOBRANCH:=v1.1.1}
OCIOCXXFLAGS=${OCIOCXXFLAGS:="-Wno-unused-function -Wno-deprecated-declarations -Wno-cast-qual -Wno-write-strings"}
# Just need libs:
OCIO_BUILDOPTS="-DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF \
               -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF \
               -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_PYGLUE=OFF \
               -DOCIO_BUILD_JAVA=OFF"
BASEDIR=`pwd`
pwd
echo "OpenColorIO install dir will be: ${OCIOINSTALLDIR}"

mkdir -p ./ext
pushd ./ext

# Clone OpenColorIO project from GitHub and build
if [ ! -e OpenColorIO ] ; then
    echo "git clone ${OCIOREPO} OpenColorIO"
    git clone ${OCIOREPO} OpenColorIO
fi
cd OpenColorIO

echo "git checkout ${OCIOBRANCH} --force"
git checkout ${OCIOBRANCH} --force
mkdir -p build
time (cd build ; cmake --config Release -DCMAKE_INSTALL_PREFIX=${OCIOINSTALLDIR} -DCMAKE_CXX_FLAGS="${OCIOCXXFLAGS}" ${OCIO_BUILDOPTS} .. && make clean && make -j 4 && make install)
popd

ls -R ${OCIOINSTALLDIR}

#echo "listing .."
#ls ..

