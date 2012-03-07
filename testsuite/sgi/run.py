#!/usr/bin/python 

imagedir = "ref/"
files = [ "norle-8.sgi", "rle-8.sgi", "norle-16.sgi", "rle-16.sgi" ]
for f in files:
    command = command + rw_command (imagedir, f)
