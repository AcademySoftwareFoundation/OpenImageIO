#!/usr/bin/env bash

# Important: set -ex causes this whole script to terminate with error if
# any command in it fails. This is crucial for CI tests.
set -ex

$OpenImageIO_ROOT/bin/oiiotool --help || true

echo "Parallel test " ${CTEST_PARALLEL_LEVEL}
pushd build/$PLATFORM
time ctest -C ${CMAKE_BUILD_TYPE} -E broken --force-new-ctest-process \
    --output-on-failure --timeout ${CTEST_TEST_TIMEOUT:=180} ${CTEST_ARGS}
popd


# if [[ "$CODECOV" == 1 ]] ; then
#     bash <(curl -s https://codecov.io/bash)
# fi
