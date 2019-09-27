#!/bin/bash

# The Linux VM used by Travis has OpenEXR 1.x. We really want 2.x.
# This script downloads and builds OpenEXR in the ext/ subdirectory.

EXRREPO=${EXRREPO:=https://github.com/openexr/openexr.git}
EXRINSTALLDIR=${EXRINSTALLDIR:=${PWD}/ext/openexr-install}
EXRBRANCH=${EXRBRANCH:=v2.3.0}
EXR_CMAKE_FLAGS=${EXR_CMAKE_FLAGS:=""}
EXR_BUILD_TYPE=${EXR_BUILD_TYPE:=Release}
EXRCXXFLAGS=${EXRCXXFLAGS:=""}
BASEDIR=$PWD
CMAKE_GENERATOR=${CMAKE_GENERATOR:="Unix Makefiles"}

pwd
echo "EXR install dir will be: ${EXRINSTALLDIR}"
echo "CMAKE_PREFIX_PATH is ${CMAKE_PREFIX_PATH}"

if [[ "$CMAKE_GENERATOR" == "" ]] ; then
    EXRGENERATOR="-G \"$CMAKE_GENERATOR\""
fi

if [[ ! -e ${EXRINSTALLDIR} ]] ; then
    mkdir -p ${EXRINSTALLDIR}
fi

# Clone OpenEXR project (including IlmBase) from GitHub and build
if [[ ! -e ./ext/openexr ]] ; then
    echo "git clone ${EXRREPO} ./ext/openexr"
    git clone ${EXRREPO} ./ext/openexr
fi

flags=

if [[ ${LINKSTATIC:=0} == 1 ]] ; then
    flags=${flags} --enable-static --enable-shared=no --with-pic
fi

pushd ./ext/openexr
echo "git checkout ${EXRBRANCH} --force"
git checkout ${EXRBRANCH} --force

if [[ ${EXRBRANCH} == "v2.4.0" ]] ; then
    # Simplified setup for 2.4+
    mkdir build
    cd build
    mkdir OpenEXR
    mkdir OpenEXR/IlmImf
    time cmake --config ${EXR_BUILD_TYPE} -G "$CMAKE_GENERATOR" \
            -DCMAKE_INSTALL_PREFIX="${EXRINSTALLDIR}" \
            -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
            -DILMBASE_PACKAGE_PREFIX=${EXRINSTALLDIR} \
            -DOPENEXR_BUILD_UTILS=0 \
            -DBUILD_TESTING=0 \
            -DPYILMBASE_ENABLE=0 \
            -DOPENEXR_VIEWERS_ENABLE=0 \
            -DCMAKE_CXX_FLAGS="${EXRCXXFLAGS}" \
            ${EXR_CMAKE_FLAGS} ..
    time cmake --build . --target install --config ${EXR_BUILD_TYPE}
elif [[ ${EXRBRANCH} == "v2.3.0" ]] ; then
    # Simplified setup for 2.3+
    mkdir build
    cd build
    mkdir OpenEXR
    mkdir OpenEXR/IlmImf
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/b44ExpLogTable.h.zip
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/dwaLookups.h.zip
    time cmake --config ${EXR_BUILD_TYPE} -G "$CMAKE_GENERATOR" -DCMAKE_INSTALL_PREFIX="${EXRINSTALLDIR}" -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" -DILMBASE_PACKAGE_PREFIX=${EXRINSTALLDIR} -DOPENEXR_BUILD_UTILS=0 -DOPENEXR_BUILD_TESTS=0 -DOPENEXR_BUILD_PYTHON_LIBS=0 -DCMAKE_CXX_FLAGS="${EXRCXXFLAGS}" ${EXR_CMAKE_FLAGS} ..
    time cmake --build . --target install --config ${EXR_BUILD_TYPE}
else
    cd IlmBase
    mkdir build
    cd build
    cmake --config ${EXR_BUILD_TYPE} ${EXRGENERATOR} -DCMAKE_INSTALL_PREFIX="${EXRINSTALLDIR}" -DCMAKE_CXX_FLAGS="${EXRCXXFLAGS}" ..
    time cmake --build . --target install
    cd ..
    cd ../OpenEXR
    cp ${BASEDIR}/src/build-scripts/OpenEXR-CMakeLists.txt CMakeLists.txt
    cp ${BASEDIR}/src/build-scripts/OpenEXR-IlmImf-CMakeLists.txt IlmImf/CMakeLists.txt
    mkdir build
    mkdir build/IlmImf
    cd build
    unzip -d IlmImf ${BASEDIR}/src/build-scripts/b44ExpLogTable.h.zip
    unzip -d IlmImf ${BASEDIR}/src/build-scripts/dwaLookups.h.zip
    cmake --config ${EXR_BUILD_TYPE} ${EXRGENERATOR} -DCMAKE_INSTALL_PREFIX="${EXRINSTALLDIR}" -DILMBASE_PACKAGE_PREFIX=${EXRINSTALLDIR} -DBUILD_UTILS=0 -DBUILD_TESTS=0 -DCMAKE_CXX_FLAGS=${EXRCXXFLAGS} ..
    time cmake --build . --target install
fi

popd

ls -R ${EXRINSTALLDIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export ILMBASE_ROOT=$EXRINSTALLDIR
export OPENEXR_ROOT=$EXRINSTALLDIR
export ILMBASE_LIBRARY_DIR=$EXRINSTALLDIR/lib
export OPENEXR_LIBRARY_DIR=$EXRINSTALLDIR/lib
export LD_LIBRARY_PATH=$OPENEXR_ROOT/lib:$LD_LIBRARY_PATH

