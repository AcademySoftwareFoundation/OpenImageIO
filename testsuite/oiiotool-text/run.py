#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os


# test --text
command += oiiotool ("--create 320x240 3 "
            "--text:x=25:y=50:font=DroidSerif \"Hello, world\" "
            "--text:x=50:y=120:color=1,0,0:size=42 \"Go Big Red!\" "
            "-d uint8 -o text.tif")

# test alignment
command += oiiotool ("--create 320x320 3 "
            "--line 90,100,110,100 --line 100,90,100,110 "
            "--line 190,100,210,100 --line 200,90,200,110 "
            "--line 90,200,110,200 --line 100,190,100,210 "
            "--line 190,200,210,200 --line 200,190,200,210 "
            "--text:x=100:y=100:xalign=left:yalign=top:size=20 Topleft "
            "--text:x=200:y=100:xalign=center:yalign=baseline:size=20 Center "
            "--text:x=100:y=200:xalign=right:yalign=bottom:size=20 Rightbot "
            "--text:x=200:y=200:xalign=left:yalign=baseline:size=20 Default "
            "-d uint8 -o aligned.tif")

# test shadow
command += oiiotool ("../common/tahoe-tiny.tif "
            "--text:x=64:y=20:xalign=center:size=20:shadow=0 \"shadow = 0\" "
            "--text:x=64:y=40:xalign=center:size=20:shadow=1 \"shadow = 1\" "
            "--text:x=64:y=60:xalign=center:size=20:shadow=2 \"shadow = 2\" "
            "-o textshadowed.tif")

# test whether alpha works properly
command += oiiotool ("--pattern checker:color1=.2,0,0:color2=0,.2,0 320x240 3 "
            "--text:x=20:y=120:color=.5,.5,.5,.5:size=50 \"Hello, world\" "
            "-d uint8 -o textalpha.tif")

# test lookup font by family and style name
command += oiiotool ("--create 320x240 3 "
            "\"--text:x=25:y=120:font=Droid Serif Bold:size=40\" \"Hello, world\" "
            "-d uint8 -o fontbyfamily.tif")

# Outputs to check against references
outputs = [ "text.tif", "aligned.tif", "textshadowed.tif", "textalpha.tif", "fontbyfamily.tif" ]

# Test Unicode characters. But only when not on Windows, because I just
# can't figure out how to get Unicode chars on the command line properly
# on Windows.
if os.name != "nt" :
    command += oiiotool ("--create 320x240 3 "
                "--text:x=25:y=50:font=DroidSerif 'test: éèúçàïö€' "
                "-d uint8 -o unicode.tif")
    outputs += [ "unicode.tif" ]

# Don't test out.txt because we have multiple reference outputs
# outputs += [ "out.txt" ]

#print "Running this command:\n" + command + "\n"
