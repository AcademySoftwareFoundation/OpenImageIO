#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Make two images that differ by a particular known pixel value
command += oiiotool("-pattern fill:color=0.1,0.1,0.1 64x64 3 -d float -o img1.exr")
command += oiiotool("-pattern fill:color=0.1,0.1,0.1 64x64 3 -d float -box:fill=1:color=0.1,0.6,0.1 5,17,15,27 -o img2.exr")

# Now make sure idiff and oiiotool --diff print the right info
failureok = True
command += diff_command("img1.exr", "img2.exr")
command += oiiotool("--diff img1.exr img2.exr")

# --pdiff
command += oiiotool("-pdiff img1.exr img2.exr")
command += oiiotool("-pdiff img1.exr img1.exr")
command += diff_command("img1.exr", "img2.exr", extraargs="-p")
command += diff_command("img1.exr", "img1.exr", extraargs="-p")
command += diff_command("img1.exr", "img2.exr", extraargs="-p -fail 1")


# Outputs to check against references
outputs = [ "out.txt" ]
