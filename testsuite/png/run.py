#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

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

# regression test for 16 bit output bug
command += oiiotool ("--pattern fill:topleft=1,0,0,1:topright=0,1,0,1:bottomleft=0,0,1,1:bottomright=1,1,1,1 16x16 4 -d uint16 -o test16.png")

# regression test for wrong gamma correction for partial alpha
command += oiiotool ("-echo alphagamma: " +
                     "--oiioattrib png:linear_premult 1 " +
                     "src/alphagamma.png --printinfo:stats=1")
command += oiiotool ("-echo gimp_gradient: src/gimp_gradient.png --printinfo:stats=1")

# Test high quality alpha deassociation using alpha value close to zero.
# This example is inspired by Yafes on the Slack.
command += oiiotool ("--pattern fill:color=0.00235,0.00106,0.00117,0.0025 1x1 4 -d uint8 -o smallalpha.png")
command += oiiotool ("--no-autopremult --dumpdata smallalpha.png")

outputs = [ "test16.png", "out.txt" ]

