#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

# command += oiiotool("-create 2x2 3 -d half -attrib tiff:RowsPerStrip -3 -o negrps.exr")
command += oiiotool("src/rowsperstrip-zero.exr -o negrps.tif")
command += info_command("negrps.tif", verbose=True, hash=True)
