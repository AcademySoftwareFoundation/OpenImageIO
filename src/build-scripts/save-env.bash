#!/usr/bin/env bash

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Save the env for use by other stages. Exclude things that we know aren't
# needed.
printenv \
    | sort \
    | grep -v ASWF_ \
    | grep -v GITHUB_ \
    | grep -v NVIDIA_ \
    | grep -v RUNNER_ \
    | grep -v _= \
    | fgrep -v "\*" \
    >> $GITHUB_ENV


echo "save-env: GITHUB_ENV=${GITHUB_ENV}"
cat $GITHUB_ENV
echo "---------"

