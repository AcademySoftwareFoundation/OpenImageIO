#!/usr/bin/env python

# Make sure we can do a read-write-read round trip of all the OpenEXR
# test images that exercise different display and pixel windows.


# ../openexr-images/DisplayWindow:
# README   t03.exr  t06.exr  t09.exr  t12.exr  t15.exr
# t01.exr  t04.exr  t07.exr  t10.exr  t13.exr  t16.exr
# t02.exr  t05.exr  t08.exr  t11.exr  t14.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/DisplayWindow"
files = [
    "t01.exr", "t02.exr", "t03.exr", "t04.exr", "t05.exr", "t06.exr",
    "t07.exr", "t08.exr", "t09.exr", "t10.exr", "t11.exr", "t12.exr",
    "t13.exr", "t14.exr", "t15.exr", "t16.exr"
]
for f in files:
    command += rw_command (imagedir, f)

