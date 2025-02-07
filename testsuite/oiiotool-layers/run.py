#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test for oiiotool channel-name-based layer splitting
#


# test --layersplit
command += oiiotool ("src/layers.exr --layersplit --siappendall -o parts.exr")

# Outputs to check against references
outputs = [ "parts.exr", "out.txt" ]

