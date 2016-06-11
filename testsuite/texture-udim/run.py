#!/usr/bin/env python


command += oiiotool ("-pattern constant:color=.5,.1,.1 256x256 3 -text:size=50:x=75:y=140 1001 -d uint8 -otex file.1001.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.1 256x256 3 -text:size=50:x=75:y=140 1002 -d uint8 -otex file.1002.tx")
command += oiiotool ("-pattern constant:color=.1,.1,.5 256x256 3 -text:size=50:x=75:y=140 1011 -d uint8 -otex file.1011.tx")
command += oiiotool ("-pattern constant:color=.1,.5,.5 256x256 3 -text:size=50:x=75:y=140 1012 -d uint8 -otex file.1012.tx")

command += testtex_command ("\"file.<UDIM>.tx\"",
                            "-nowarp -scalest 2 2 --iters 100 -res 128 128 -d uint8 -o out.tif")
#command += diff_command ("out.tif", "ref/out.tif", "--fail 0.0005 --warn 0.0005")

outputs = [ "out.tif" ]
