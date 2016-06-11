#!/usr/bin/env python


command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 u1v1 -d uint8 -otex file_u1_v1.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 u2v1 -d uint8 -otex file_u2_v1.tx")
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 u1v2 -d uint8 -otex file_u1_v2.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 u2v2 -d uint8 -otex file_u2_v2.tx")

command += testtex_command ("\"file_<U>_<V>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out.tif")
#command += diff_command ("out.tif", "ref/out.tif", "--fail 0.0005 --warn 0.0005")

outputs = [ "out.tif" ]
