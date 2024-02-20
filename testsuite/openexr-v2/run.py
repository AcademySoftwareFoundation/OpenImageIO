#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/v2/Stereo"

# Multi-part, not deep
command += rw_command (imagedir, "composited.exr", use_oiiotool=1,
                       preargs="--stats")

# Multi-part and also deep
command += rw_command (imagedir, "Balls.exr", use_oiiotool=1,
                       preargs="--stats")

# Convert from scanline to tiled
command += rw_command (imagedir, "Leaves.exr", use_oiiotool=1,
                       extraargs="--tile 64 64 ", preargs="--stats")

# Check a complicated multipart example
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Beachball"
files = [ "multipart.0001.exr" ]
for f in files:
    command += rw_command (imagedir, f, use_oiiotool=1, extraargs="-a")
