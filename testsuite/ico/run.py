#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

failureok = 1
redirect = ' >> out.txt 2>&1 '

command += rw_command (OIIO_TESTSUITE_IMAGEDIR, "oiio.ico")
command += iconvert ("src/bad1.ico out.tif")
command += iconvert ("src/crash-002c.ico out.tif")
command += iconvert ("src/bad-not-8x-wide.ico out.tif")
command += iconvert ("src/bad-palette.ico out.tif")
# ICO embedding a PNG whose IHDR declares a ~6 GB image (46340x46340). The
# compression-ratio guard must reject it before the PNG buffer is allocated.
command += iconvert ("src/bomb-png-46340.ico out.tif")

