#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

############################################################################
# Reading an EXR a few scanlines at a time must return the same pixels as
# reading the whole image, no matter how the requested swaths line up with
# the compressed chunks of the file (which hold 1, 16, 32, or 256 scanlines
# depending on the compression).
############################################################################

redirect = " >> out.txt 2>&1 "

# Test images: a few compressions with different scanlines per chunk, and
# one with a data window that doesn't start at y=0.
for comp in ["none", "zips", "zip", "piz", "dwab"]:
    command += oiiotool("--pattern noise:type=uniform:seed=1 41x300 4 "
                        + "--compression " + comp + " -d half -o " + comp + ".exr")
command += oiiotool("--pattern noise:type=uniform:seed=1 41x300 4 --origin -7-13 "
                    + "--compression zip -d half -o offset.exr")

command += pythonbin + " src/test_scanlines.py >> out.txt 2>&1 ;"

outputs = ["out.txt"]
