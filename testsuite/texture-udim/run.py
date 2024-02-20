#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 1001 --attrib Make pet --attrib Model dog -d uint8 -otex file.1001.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 1002 --attrib Make pet --attrib Model dog -d uint8 -otex file.1002.tx")
# force DateTime to differ in these files
command += "sleep 1 ; "
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 1011 --attrib Make pet --attrib Model cat -d uint8 -otex file.1011.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 1012 --attrib Make pet --attrib Model dog -d uint8 -otex file.1012.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 1032 --attrib Make pet --attrib Model dog -d uint8 -otex file.1032.tx")

command += testtex_command ("\"file.<UDIM>.tx\"",
                            "-nowarp -scalest 2 2 -res 128 128 -d uint8 -o out.tif")

# Test get_texture_info on one thing that should be identical across all
# the tiles, and one thing that should not be.
command += testtex_command ("\"file.<UDIM>.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file.<UDIM>.tx\" --gettextureinfo Model --iters 0")

# Test the other form of single-index udim pattern
command += testtex_command ("\"file.%(UDIM)d.tx\"",
                            "-nowarp -scalest 2 2 -res 128 128 -d uint8 -o out2.tif")
command += testtex_command ("\"file.%(UDIM)d.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file.%(UDIM)d.tx\" --gettextureinfo Model --iters 0")

# Exercise making making temp files, and having low file and tile limits
command += testtex_command ("--udim --maketests 10 \"mktest_{:04}.<UDIM>.tx\""
                            + " --maketest-res 1024 --cachesize 1 --maxfiles 5"
                            + " --res 256 256 -d uint8 -o out3.tif")

outputs = [ "out.tif", "out2.tif", "out3.tif", "out.txt" ]
