#!/usr/bin/env python

imagedir = parent + "/oiio-images"
command += oiiotool (imagedir+"/grid.tif --scanline -o grid.iff")
command += diff_command (imagedir+"/grid.tif", "grid.iff")
