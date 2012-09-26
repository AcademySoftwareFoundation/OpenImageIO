#!/usr/bin/python 

imagedir = parent + "/openexr-images/v2/Stereo"

# Multi-part, not deep
command += rw_command (imagedir, "composited.exr", use_oiiotool=1,
                       preargs="--stats")

# Multi-part and also deep
command += rw_command (imagedir, "Balls.exr", use_oiiotool=1,
                       preargs="--stats")

# Convert from scanline to tiled
command += rw_command (imagedir, "Leaves.exr", use_oiiotool=1,
                       extraargs="--tile 64 64 ", preargs="--stats")
