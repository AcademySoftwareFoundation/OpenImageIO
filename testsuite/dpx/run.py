#!/usr/bin/python 

imagedir = parent + "/oiio-images"
files = [ "dpx_nuke_10bits_rgb.dpx", "dpx_nuke_16bits_rgba.dpx" ]
for f in files:
    command += rw_command (imagedir, f)
