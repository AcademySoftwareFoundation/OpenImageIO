#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -e

# Arguments to this script are: 
#      BUILDDIR_NEW BUILDDIR_OLD LIBRARIES...

BUILDDIR_NEW=$1
shift
BUILDDIR_OLD=$1
shift
LIBS=$*

#
# First, create ABI dumps from both builds
#
ABI_ARGS="-bin-only -skip-cxx -public-headers $PWD/dist/include/OpenImageIO "
echo "ABI_CHECK: PWD=${PWD} "
ls -l $BUILDDIR_NEW
ls -l $BUILDDIR_OLD
for dir in $BUILDDIR_NEW $BUILDDIR_OLD ; do
    for lib in $LIBS ; do
        abi-dumper $ABI_ARGS ${dir}/lib/${lib}.so -o ${dir}/abi-${lib}.dump
    done
done
echo "Saved ABI dumps"

#
# Run the ABI compliance checker, saving the outputs to files
#
for lib in $LIBS ; do
    abi-compliance-checker -l $lib -old $BUILDDIR_OLD/abi-$lib.dump -new $BUILDDIR_NEW/abi-$lib.dump | tee ${lib}-abi-results.txt || true
    echo -e "\x1b[33;1m"
    echo -e "$lib"
    fgrep "Binary compatibility:" ${lib}-abi-results.txt
    echo -e "\x1b[33;0m"
done

#
# If the "Binary compatibility" summary results say anything other than 100%,
# we fail!
#
for lib in $LIBS ; do
    if [[ `fgrep "Binary compatibility:" ${lib}-abi-results.txt | grep -v 100\%` != "" ]] ; then
        cp -r compat_reports ${BUILDDIR_NEW}/compat_reports
        exit 1
    fi
done
