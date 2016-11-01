#!/usr/bin/env python

imagedir = parent + "/oiio-images"
command += oiiotool (imagedir+"/grid.tif --scanline -o gridscanline.iff")
command += diff_command (imagedir+"/grid.tif", "gridscanline.iff")
command += oiiotool (imagedir+"/grid.tif --tile 64 64 -o gridtile.iff")
command += diff_command (imagedir+"/grid.tif", "gridtile.iff")
