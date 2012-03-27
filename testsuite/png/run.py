#!/usr/bin/python

imagedir = parent + "/oiio-images"
files = [ "oiio-logo-no-alpha.png",  "oiio-logo-with-alpha.png" ]
for f in files:
        command += rw_command (imagedir,  f)
