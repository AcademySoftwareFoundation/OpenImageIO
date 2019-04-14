#!/usr/bin/env bash

# This script is run when CI system first starts up.
# It expects that ci-setenv.bash was run first, so $PLATFORM and $ARCH
# have been set.

if [[ -e src/build-scripts/ci-setenv.bash ]] ; then
    source src/build-scripts/ci-setenv.bash
fi


if [[ "$TRAVIS" != "" ]] ; then
    MAKEFLAGS=-j2
elif [[ "$CIRCLECI" != "" ]] ; then
    MAKEFLAGS=-j4
fi

make $MAKEFLAGS VERBOSE=1 $BUILD_FLAGS cmakesetup
make $MAKEFLAGS $BUILD_FLAGS $BUILDTARGET

if [[ "$SKIP_TESTS" == "" ]] ; then
    $OPENIMAGEIO_ROOT_DIR/bin/oiiotool --help
    make $BUILD_FLAGS test
fi

if [[ $BUILDTARGET == clang-format ]] ; then
    git diff --color
    THEDIFF=`git diff`
    if [[ "$THEDIFF" != "" ]] ; then
        echo "git diff was not empty. Failing clang-format or clang-tidy check."
        exit 1
    fi
fi

if [[ "$CODECOV" == 1 ]] ; then
    bash <(curl -s https://codecov.io/bash)
fi
