#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

CLANG_FORMAT_EXE=${CLANG_FORMAT_EXE:="clang-format"}
echo "Running " `which clang-format` " version " `${CLANG_FORMAT_EXE} --version`

files=`find ./{src,testsuite} \( -name '*.h' -o -name '*.cpp' \) -print \
       | grep -Ev 'pugixml|SHA1|farmhash.cpp|libdpx|libcineon|bcdec.h|gif.h|stb_sprintf.h'`


${CLANG_FORMAT_EXE}  -i -style=file $files
git diff --color
THEDIFF=`git diff`
if [[ "$THEDIFF" != "" ]] ; then
    echo "git diff was not empty. Failing clang-format check."
    exit 1
fi
