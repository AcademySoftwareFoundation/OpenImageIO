#!/usr/bin/python 

imagedir = parent + "/openexr-images/v2/Stereo"
files = [ "composited.exr" ]
for f in files:
    command += rw_command (imagedir, f, use_oiiotool=1)

