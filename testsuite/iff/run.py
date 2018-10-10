#!/usr/bin/env python

command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --scanline -o gridscanline.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridscanline.iff")
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --tile 64 64 -o gridtile.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridtile.iff")
