#!/usr/bin/python 
imagedir = parent + "/oiio-images"
files = ["g01.xpm", "g02.xpm", "g03.xpm"]
for f in files:
    command += rw_command (imagedir, f, 0)
