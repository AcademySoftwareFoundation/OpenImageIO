#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
# (Though we let it run all the way through for code coverage workflows.)
if [[ "${CODECOV}" == "" ]]; then
    set -ex
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OpenImageIO_ROOT/lib
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$OpenImageIO_ROOT/lib/pkgconfig
export OIIO_INCLUDE_DIR=$OpenImageIO_ROOT/include
export OIIO_LIBRARY_DIR=$OpenImageIO_ROOT/lib

echo "Using C++ STD ${OIIO_CXX_STANDARD}"

echo "Running Rust oiio-sys tests"

pushd src/rust/oiio-sys
time cargo test --all-features
popd

echo "Running Rust oiio tests"

pushd src/rust
time cargo test --all-features
popd
