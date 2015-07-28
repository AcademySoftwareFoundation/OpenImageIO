#!/usr/bin/env python

imagedir = "src"
files = [ "triangle.ptx" ]
for f in files:
    command += info_command (imagedir + "/" + f)
