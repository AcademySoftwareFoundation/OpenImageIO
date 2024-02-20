#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test reading and writing of files with "rational" metadata


# Round trip of a file known to have some rational values
command += rw_command ("src", "test.exr")


# Try setting rational metadata as "a/b" on the oiiotool command line
command += oiiotool ("--create 64x64 3 --attrib:type=rational onehalf 50/100 -o rat2.exr")
command += info_command ("rat2.exr", safematch=True)
