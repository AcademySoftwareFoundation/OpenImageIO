#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test ability to read a JPEG YCbCrK encoded file
command += info_command ("src/YCbCrK.jpg", safematch=True)
command += oiiotool ("src/YCbCrK.jpg -o rgb-from-YCbCrK.tif")

outputs = [ "rgb-from-YCbCrK.tif", "out.txt" ]
