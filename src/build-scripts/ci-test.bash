#!/usr/bin/env bash

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

: ${CTEST_EXCLUSIONS:="broken"}
: ${CTEST_TEST_TIMEOUT:=180}

$OpenImageIO_ROOT/bin/oiiotool --help

echo "Parallel test ${CTEST_PARALLEL_LEVEL}"
echo "Default timeout ${CTEST_TEST_TIMEOUT}"
echo "Test exclusions '${CTEST_EXCLUSIONS}'"
echo "CTEST_ARGS '${CTEST_ARGS}'"

pushd build
time ctest -C ${CMAKE_BUILD_TYPE} --force-new-ctest-process --output-on-failure \
    -E "${CTEST_EXCLUSIONS}" --timeout ${CTEST_TEST_TIMEOUT} ${CTEST_ARGS}
popd
