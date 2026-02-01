#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

BUILD_BIN_DIR=build/bin
if [[ "${RUNNER_OS}" == "Windows" ]] ; then
    BUILD_BIN_DIR+=/${CMAKE_BUILD_TYPE}
fi

ls build
ls $BUILD_BIN_DIR

mkdir -p build/benchmarks
for t in image_span_test span_test ; do
    echo
    echo
    echo "$t"
    echo "========================================================"
    OpenImageIO_CI=0 ${BUILD_BIN_DIR}/$t | tee build/benchmarks/$t.out
    # Note: set OpenImageIO_CI=0 to avoid CI-specific automatic reduction of
    # the number of trials and iterations.
    echo "========================================================"
    echo "========================================================"
    echo
done
