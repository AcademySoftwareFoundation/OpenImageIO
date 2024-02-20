#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 u1v1 --attrib Make pet --attrib Model dog -d uint8 -otex file_u1v1.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 u2v1 --attrib Make pet --attrib Model dog -d uint8 -otex file_u2v1.tx")
# force DateTime to differ in these files
command += "sleep 1 ; "
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 u1v2 --attrib Make pet --attrib Model cat -d uint8 -otex file_u1v2.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 u2v2 --attrib Make pet --attrib Model dog -d uint8 -otex file_u2v2.tx")

command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 u1v1 --attrib Make pet --attrib Model dog -d uint8 -otex file0_u0v0.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 u2v1 --attrib Make pet --attrib Model dog -d uint8 -otex file0_u1v0.tx")
# force DateTime to differ in these files
command += "sleep 1 ; "
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 u1v2 --attrib Make pet --attrib Model cat -d uint8 -otex file0_u0v1.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 u2v2 --attrib Make pet --attrib Model dog -d uint8 -otex file0_u1v1.tx")

command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 u1v1 --attrib Make pet --attrib Model dog -d uint8 -otex file0.u1_v1.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 u2v1 --attrib Make pet --attrib Model dog -d uint8 -otex file0.u2_v1.tx")
# force DateTime to differ in these files
command += "sleep 1 ; "
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 u1v2 --attrib Make pet --attrib Model cat -d uint8 -otex file0.u1_v2.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 u2v2 --attrib Make pet --attrib Model dog -d uint8 -otex file0.u2_v2.tx")

# Textures for <uvtile>
command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 u1v1 --attrib Make pet --attrib Model dog -d uint8 -otex file1.u0_v0.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 u2v1 --attrib Make pet --attrib Model dog -d uint8 -otex file1.u1_v0.tx")
# force DateTime to differ in these files
command += "sleep 1 ; "
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 u1v2 --attrib Make pet --attrib Model cat -d uint8 -otex file1.u0_v1.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 u2v2 --attrib Make pet --attrib Model dog -d uint8 -otex file1.u1_v1.tx")

command += testtex_command ("\"file_<U><V>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out.tif")
#command += diff_command ("out.tif", "ref/out.tif", "--fail 0.0005 --warn 0.0005")

# Test get_texture_info on one thing that should be identical across all
# the tiles, and one thing that should not be.
command += testtex_command ("\"file_<U><V>.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file_<U><V>.tx\" --gettextureinfo Model --iters 0")

# Test the other forms of 2-index udim patterns
command += testtex_command ("\"file0_u##v##.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out2.tif")
command += testtex_command ("\"file0_u##v##.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file0_u##v##.tx\" --gettextureinfo Model --iters 0")

command += testtex_command ("\"file0_<u><v>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out3.tif")
command += testtex_command ("\"file0_<u><v>.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file0_<u><v>.tx\" --gettextureinfo Model --iters 0")

command += testtex_command ("\"file0.<UVTILE>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out4.tif")
command += testtex_command ("\"file0.<UVTILE>.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file0.<UVTILE>.tx\" --gettextureinfo Model --iters 0")

command += testtex_command ("\"file1.<uvtile>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out5.tif")
command += testtex_command ("\"file1.<uvtile>.tx\" --gettextureinfo Make --iters 0")
command += testtex_command ("\"file1.<uvtile>.tx\" --gettextureinfo Model --iters 0")

outputs = [ "out.tif", "out2.tif", "out3.tif", "out4.tif", "out5.tif", "out.txt" ]
