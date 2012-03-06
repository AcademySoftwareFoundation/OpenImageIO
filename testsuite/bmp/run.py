#!/usr/bin/python 

imagedir = parent + "/bmpsuite"
files = [ "g01bg.bmp", "g04.bmp", "g08.bmp",
          "g16bf555.bmp", "g24.bmp", "g32bf.bmp" ]
for f in files :
    command += rw_command (imagedir, f)
