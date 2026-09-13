#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Regression test for a bug where
#  - TIFF file with a readable subimage, and a second corrupted subimage
#  - succeeds reading subimage 0
#  - fails on seek to subimage 1
#  - but then is confused about which subimage it's on
# This test reproduces those conditions to make sure the bug is fixed.

command += pythonbin + " src/test_subimage_seek.py > out.txt ;"

outputs = [ "out.txt" ]
