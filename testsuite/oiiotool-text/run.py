#!/usr/bin/env python


# test --text
command += oiiotool ("--create 320x240 3 "
            "--text:x=25:y=50:font=DroidSerif 'Hello, world' "
            "--text:x=50:y=120:color=1,0,0:size=42 \"Go Big Red!\" "
            "-d uint8 -o text.tif >> out.txt")


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "text.tif" ]

#print "Running this command:\n" + command + "\n"
