#!/usr/bin/env python

imagedir = parent + "/bmpsuite"
files = [ "g01bg.bmp", "g04.bmp", "g08.bmp",
          "g16bf555.bmp", "g24.bmp", "g32bf.bmp" ]
for f in files :
    command += rw_command (imagedir, f)

# Test BMR version 5
command += rw_command ("src", "g01bg2-v5.bmp")
