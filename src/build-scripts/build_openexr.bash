#!/bin/bash

# Which OpenEXR to retrieve, how to build it
OPENEXR_REPO=${OPENEXR_REPO:=https://github.com/openexr/openexr.git}
OPENEXR_BRANCH=${OPENEXR_BRANCH:=v2.4.0}

# Where to install the final results
OPENEXR_INSTALL_DIR=${OPENEXR_INSTALL_DIR:=${PWD}/ext/openexr-install}
OPENEXR_BUILD_TYPE=${OPENEXR_BUILD_TYPE:=Release}
CMAKE_GENERATOR=${CMAKE_GENERATOR:="Unix Makefiles"}
OPENEXR_CMAKE_FLAGS=${OPENEXR_CMAKE_FLAGS:=""}
OPENEXR_CXX_FLAGS=${OPENEXR_CXX_FLAGS:=""}
BASEDIR=$PWD

pwd
echo "Building OpenEXR ${OPENEXR_BRANCH}"
echo "EXR install dir will be: ${OPENEXR_INSTALL_DIR}"
echo "CMAKE_PREFIX_PATH is ${CMAKE_PREFIX_PATH}"
echo "OpenEXR Build type is ${OPENEXR_BUILD_TYPE}"

if [[ "$CMAKE_GENERATOR" == "" ]] ; then
    OPENEXR_GENERATOR_CMD="-G \"$CMAKE_GENERATOR\""
fi

if [[ ! -e ${OPENEXR_INSTALL_DIR} ]] ; then
    mkdir -p ${OPENEXR_INSTALL_DIR}
fi

# Clone OpenEXR project (including IlmBase) from GitHub and build
if [[ ! -e ./ext/openexr ]] ; then
    echo "git clone ${OPENEXR_REPO} ./ext/openexr"
    git clone ${OPENEXR_REPO} ./ext/openexr
fi

pushd ./ext/openexr
git checkout ${OPENEXR_BRANCH} --force

if [[ ${OPENEXR_BRANCH} == "v2.2.0" ]] || [[ ${OPENEXR_BRANCH} == "v2.2.1" ]] ; then
    cd IlmBase
    mkdir build
    cd build
    cmake --config ${OPENEXR_BUILD_TYPE} ${OPENEXR_GENERATOR_CMD} \
            -DCMAKE_INSTALL_PREFIX="${OPENEXR_INSTALL_DIR}" \
            -DCMAKE_CXX_FLAGS="${OPENEXR_CXX_FLAGS}" ..
    time cmake --build . --target install --config ${OPENEXR_BUILD_TYPE}
    cd ../../OpenEXR
    cp ${BASEDIR}/src/build-scripts/OpenEXR-CMakeLists.txt CMakeLists.txt
    cp ${BASEDIR}/src/build-scripts/OpenEXR-IlmImf-CMakeLists.txt IlmImf/CMakeLists.txt
    mkdir -p build/IlmImf
    cd build
    unzip -d IlmImf ${BASEDIR}/src/build-scripts/b44ExpLogTable.h.zip
    unzip -d IlmImf ${BASEDIR}/src/build-scripts/dwaLookups.h.zip
    cmake --config ${OPENEXR_BUILD_TYPE} ${OPENEXR_GENERATOR_CMD} \
            -DCMAKE_INSTALL_PREFIX="${OPENEXR_INSTALL_DIR}" \
            -DILMBASE_PACKAGE_PREFIX=${OPENEXR_INSTALL_DIR} \
            -DBUILD_UTILS=0 -DBUILD_TESTS=0 \
            -DCMAKE_CXX_FLAGS="${OPENEXR_CXX_FLAGS}" \
            ${OPENEXR_CMAKE_FLAGS} ..
    time cmake --build . --target install --config ${OPENEXR_BUILD_TYPE}
elif [[ ${OPENEXR_BRANCH} == "v2.3.0" ]] ; then
    # Simplified setup for 2.3+
    mkdir -p build/OpenEXR/IlmImf && true
    cd build
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/b44ExpLogTable.h.zip
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/dwaLookups.h.zip
    cmake --config ${OPENEXR_BUILD_TYPE} -G "$CMAKE_GENERATOR" \
            -DCMAKE_INSTALL_PREFIX="${OPENEXR_INSTALL_DIR}" \
            -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
            -DILMBASE_PACKAGE_PREFIX=${OPENEXR_INSTALL_DIR} \
            -DOPENEXR_BUILD_UTILS=0 \
            -DOPENEXR_BUILD_TESTS=0 \
            -DOPENEXR_BUILD_PYTHON_LIBS=0 \
            -DCMAKE_CXX_FLAGS="${OPENEXR_CXX_FLAGS}" \
            ${OPENEXR_CMAKE_FLAGS} ..
    time cmake --build . --target install --config ${OPENEXR_BUILD_TYPE}
else
    # Simplified setup for 2.4+
    mkdir -p build/OpenEXR/IlmImf && true
    cd build
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/b44ExpLogTable.h.zip
    unzip -d OpenEXR/IlmImf ${BASEDIR}/src/build-scripts/dwaLookups.h.zip
    cmake --config ${OPENEXR_BUILD_TYPE} -G "$CMAKE_GENERATOR" \
            -DCMAKE_INSTALL_PREFIX="${OPENEXR_INSTALL_DIR}" \
            -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
            -DILMBASE_PACKAGE_PREFIX=${OPENEXR_INSTALL_DIR} \
            -DOPENEXR_BUILD_UTILS=0 \
            -DBUILD_TESTING=0 \
            -DPYILMBASE_ENABLE=0 \
            -DOPENEXR_VIEWERS_ENABLE=0 \
            -DCMAKE_CXX_FLAGS="${OPENEXR_CXX_FLAGS}" \
            ${OPENEXR_CMAKE_FLAGS} ..
    time cmake --build . --target install --config ${OPENEXR_BUILD_TYPE}
fi

popd

ls -R ${OPENEXR_INSTALL_DIR}

#echo "listing .."
#ls ..

# Set up paths. These will only affect the caller if this script is
# run with 'source' rather than in a separate shell.
export ILMBASE_ROOT=$OPENEXR_INSTALL_DIR
export OPENEXR_ROOT=$OPENEXR_INSTALL_DIR
export ILMBASE_LIBRARY_DIR=$OPENEXR_INSTALL_DIR/lib
export OPENEXR_LIBRARY_DIR=$OPENEXR_INSTALL_DIR/lib
export LD_LIBRARY_PATH=$OPENEXR_ROOT/lib:$LD_LIBRARY_PATH

