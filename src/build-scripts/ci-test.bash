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

: ${CTEST_EXCLUSIONS:="broken"}
: ${CTEST_TEST_TIMEOUT:=180}

#
# Try a few command line options to test them and also get some important
# debugging info in the CI logs.
#
echo ; echo "Results of oiiotool --version:"
$OpenImageIO_ROOT/bin/oiiotool --version || true
echo ; echo "Results of oiiotool brief help:"
$OpenImageIO_ROOT/bin/oiiotool || true
echo ; echo "Results of oiiotool full --help:"
$OpenImageIO_ROOT/bin/oiiotool --help || true
echo ; echo "Results of oiiotool --colorconfiginfo:"
$OpenImageIO_ROOT/bin/oiiotool --colorconfiginfo
echo ; echo "Results of oiiotool with no args (should get short help message):"
$OpenImageIO_ROOT/bin/oiiotool || true
echo ; echo "Run unit tests and simple stats:"
$OpenImageIO_ROOT/bin/oiiotool --unittest --list-formats --threads 0 \
                 --cache 1000 --autotile --autopremult --runstats || true
echo ; echo "Try unknown command:"
$OpenImageIO_ROOT/bin/oiiotool -q --unknown || true


#
# Full test suite
#
echo "Parallel test ${CTEST_PARALLEL_LEVEL}"
echo "Default timeout ${CTEST_TEST_TIMEOUT}"
echo "Test exclusions '${CTEST_EXCLUSIONS}'"
echo "CTEST_ARGS '${CTEST_ARGS}'"

pushd build
time ctest -C ${CMAKE_BUILD_TYPE} --force-new-ctest-process --output-on-failure \
    -E "${CTEST_EXCLUSIONS}" --timeout ${CTEST_TEST_TIMEOUT} ${CTEST_ARGS}
popd
