#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --scanline -o gridscanline.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridscanline.iff")
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --tile 64 64 -o gridtile.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridtile.iff")

# Regression test: verify reading of 16 bit rgba + float z (used to have a
# buffer overrun)
command += info_command("src//tiny_rgba16z.iff", hash=True)
