#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Run code coverage analysis
# This assumes that the build occurred with CODECOV=1 and tests have already
# fully run.

set -ex

echo "Performing code coverage analysis"
mkdir _coverage
pushd _coverage


# The sed command below converts from:
#   ../build/src/libOpenImageIO/CMakeFiles/OpenImageIO.dir/foo.gcno
# to:
#   ../src/libOpenImageIO/foo.cpp

for g in $(find ../build -name "*.gcno" -type f); do
    echo "Processing $g"
    echo "dirname $g = $(dirname $g) to " `$(echo "$g" | sed -e 's/\/build\//\//' -e 's/\.gcno/\.cpp/' -e 's/\.cpp\.cpp/\.cpp/' -e 's/\/CMakeFiles.*\.dir\//\//')`
    gcov -l -p -o $(dirname "$g") $(echo "$g" | sed -e 's/\/build\//\//' -e 's/\.gcno/\.cpp/' -e 's/\/CMakeFiles.*\.dir\//\//')
done

# Remove pointless .gcov files so we don't analyze them
rm -f "*opt*include*"
rm -f "*usr*local*"

popd
