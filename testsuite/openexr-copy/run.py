#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os, sys

####################################################################
# This test exercises oiiotool functionality that is mostly about
# copying pixels from one image to another.
####################################################################

# Capture error output
redirect = " >> out.txt 2>&1 "

# Create some test images we need
command += oiiotool("../common/tahoe-tiny.tif --compression pxr24 -o compressed-pxr24.exr")
command += oiiotool("../common/tahoe-tiny.tif --compression b44 -o compressed-b44.exr")
command += oiiotool("../common/tahoe-tiny.tif --compression b44a -o compressed-b44a.exr")
command += oiiotool("../common/tahoe-tiny.tif --compression dwaa -o compressed-dwaa.exr")
command += oiiotool("../common/tahoe-tiny.tif --compression dwab -o compressed-dwab.exr")

# Run the recompression test script 
command += pythonbin + " src/test_recompression.py;"

# Outputs to check against references
outputs = [
    "compressed-pxr24.exr",
    "compressed-b44.exr",
    "compressed-b44a.exr",
    "compressed-dwaa.exr",
    "compressed-dwab.exr",
    "out.txt"
]

# OpenEXRInputCore is not supported yet. TODO: update this once it's implemented.
if os.environ.get("OPENIMAGEIO_OPTIONS") == "openexr:core=1":
    outputs = []

#print "Running this command:\n" + command + "\n"
