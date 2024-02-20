#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test "missingcolor" attribute by reading a tiled exr that deliberately has
# missing tiles. Make sure that we do the right thing in the presence of
# missingcolor attribute.

# change redirection to send stderr to a separate file
redirect = " >> out.txt 2>> out.err.txt "
failureok = 1

# This command should fail with errors about missing tiles
command += oiiotool("src/partial.exr -d uint8 -o error.tif")
command += oiiotool("-iconfig oiio:missingcolor \"-1,0,0\" src/partial.exr -d uint8 -o missing.tif")

outputs = [ "missing.tif", "out.txt", "out.err.txt" ]
