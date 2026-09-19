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
# Things to exclude from the comparison. Both are Perl regexes matched against
# any part of a (demangled) type name / (mangled) symbol name, and both can be
# set per-CI-job (e.g. via the job's `setenvs:`), since different ABI baselines
# (vs 3.1, vs 3.2, ...) legitimately want to ignore different things:
#
#   ABI_SKIP_TYPES_RE    types whose internal changes should not be reported
#   ABI_SKIP_SYMBOLS_RE  symbols whose changes/removals should not be reported
#
# `ABI_SKIP_TYPES_RE` defaults to the span family: `span<T>` (and hence
# `cspan<T>`, which is just `span<const T>`) is always a pointer + size, by
# design tracking std::span, so its layout will never change -- but abi-dumper
# picks a representative among the layout-identical `span<...>` instantiations
# that varies between builds, so acc periodically reports a spurious "base type
# has been changed ... of different format" diff on it.
#
# (Use ${VAR-default} so a job can force "no skipping" with an explicit empty
# value, e.g. `setenvs: export ABI_SKIP_TYPES_RE=`.)
ABI_SKIP_TYPES_RE=${ABI_SKIP_TYPES_RE-'span<'}
ABI_SKIP_SYMBOLS_RE=${ABI_SKIP_SYMBOLS_RE-}

ABI_SKIP_ARGS=""
if [[ -n $ABI_SKIP_TYPES_RE ]] ; then
    ABI_SKIP_ARGS+=" -skip-internal-types $ABI_SKIP_TYPES_RE"
fi
if [[ -n $ABI_SKIP_SYMBOLS_RE ]] ; then
    ABI_SKIP_ARGS+=" -skip-internal-symbols $ABI_SKIP_SYMBOLS_RE"
fi
echo "ABI_CHECK: skip args:${ABI_SKIP_ARGS:- none}"

#
# Run the ABI compliance checker, saving the outputs to files
#
for lib in $LIBS ; do
    abi-compliance-checker -l $lib -old $BUILDDIR_OLD/abi-$lib.dump -new $BUILDDIR_NEW/abi-$lib.dump $ABI_SKIP_ARGS | tee ${lib}-abi-results.txt || true
    echo -e "\x1b[33;1m"
    echo -e "$lib"
    fgrep "Binary compatibility:" ${lib}-abi-results.txt
    echo -e "\x1b[33;0m"
done
cp -r compat_reports ${BUILDDIR_NEW}/compat_reports || true

#
# If the "Binary compatibility" summary results say anything other than 100%,
# we fail!
#
for lib in $LIBS ; do
    if [[ `fgrep "Binary compatibility:" ${lib}-abi-results.txt | grep -v 100\%` != "" ]] ; then
        exit 1
    fi
done
