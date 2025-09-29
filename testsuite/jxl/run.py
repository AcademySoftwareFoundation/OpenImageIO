#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Test oiiotool's ability to add and delete attributes


# Test adding and extracting ICC profiles
command += oiiotool ("../common/tahoe-tiny.tif --iccread ref/test-jxl.icc -o tahoe-icc.jxl")
command += info_command ("tahoe-icc.jxl", safematch=True)
command += oiiotool ("tahoe-icc.jxl --iccwrite test-jxl.icc")

outputs = [
            "test-jxl.icc",
            "out.txt"
]
