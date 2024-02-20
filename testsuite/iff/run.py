#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --scanline -o gridscanline.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridscanline.iff")
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --tile 64 64 -o gridtile.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridtile.iff")
