#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test adding and extracting ICC profiles
command += oiiotool ("../common/tahoe-tiny.tif --iccread ref/test-jxl.icc -o tahoe-icc.jxl")
command += info_command ("tahoe-icc.jxl", safematch=True)
command += oiiotool ("tahoe-icc.jxl --iccwrite test-jxl.icc")

command += oiiotool ("../common/tahoe-tiny.tif --cicp \"9,16,9,1\" -o tahoe-cicp-pq.jxl")
command += info_command ("tahoe-cicp-pq.jxl", safematch=True)

outputs = [
            "test-jxl.icc",
            "out.txt"
]
