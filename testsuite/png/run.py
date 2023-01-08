#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "

files = [ "oiio-logo-no-alpha.png",  "oiio-logo-with-alpha.png" ]
for f in files:
        command += rw_command (OIIO_TESTSUITE_IMAGEDIR,  f)

# test writing of exif data
command += oiiotool ("--create 64x64 3 " +
                     "--attrib Exif:WhiteBalance 0 " +
                     "--attrib Exif:FocalLength 45.7 " +
                     "-o exif.png")
command += info_command ("exif.png", safematch=True)