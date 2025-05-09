#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

mkdir -p build/benchmarks
for t in image_span_test ; do
    echo
    echo
    echo "$t"
    echo "========================================================"
    build/bin/$t > build/benchmarks/$t.out
    cat build/benchmarks/$t.out
    echo "========================================================"
    echo "========================================================"
    echo
done
