#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os, sys

####################################################################
# Verify decreasingY line order generates same image as increasingY (default).
####################################################################

# Capture error output
redirect = " >> out.txt 2>&1 "

# Create reference images stored in increasingY order (default)
# Use a large image size to ensure scanline chunking logic is triggered.
command += oiiotool("-pattern:type=half fill:topleft=1,0,0:topright=0,1,0:bottomleft=0,0,1:bottomright=1,1,1 4080x3072 3 -o increasingY.exr")
command += info_command("increasingY.exr", safematch=True, hash=True, verbose=False)
# Just do a straight copy of it via oiiotool, all default operations
command += oiiotool("increasingY.exr -o increasingY-copy.exr")
command += info_command("increasingY-copy.exr", safematch=True, hash=True, verbose=False)

# Create an image in decreasing order (Tests ImageOutput::write_image() logic)
command += oiiotool("-pattern:type=half fill:topleft=1,0,0:topright=0,1,0:bottomleft=0,0,1:bottomright=1,1,1 4080x3072 3 --attrib openexr:lineOrder decreasingY -o decreasingY.exr")
command += info_command("decreasingY.exr", safematch=True, hash=True, verbose=False)

# Create an image in decreasing order via copying a reference image. (Tests ImageBuf::write() logic)
command += oiiotool("decreasingY.exr --attrib openexr:lineOrder decreasingY -o decreasingY-copy.exr")
command += info_command("decreasingY-copy.exr", safematch=True, hash=True, verbose=False)

# The hashes should all match!

# Outputs to check against references.
outputs = [
    "out.txt"
]
