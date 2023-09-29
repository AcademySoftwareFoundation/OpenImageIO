#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Basic test -- save with dither
command += oiiotool ("-pattern fill:left=0,0,0:right=0.0625,0.0625,0.0625 256x256 3 -d uint8 -dither -o ramp.tif")

# Regression test for a buffer copy bug
command += oiiotool ("src/copybug-input.exr -ch R -dither -d uint8 -o bad.tif")

outputs = [ "ramp.tif", "bad.tif" ]
