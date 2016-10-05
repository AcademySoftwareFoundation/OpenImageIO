#!/usr/bin/env python 

# Basic test -- python script that saves with dither
command += "python src/make_ramp.py > out.txt ;"

# Regression test for a buffer copy bug
command += oiiotool ("src/copybug-input.exr -ch R -dither -d uint8 -o bad.tif")

outputs = [ "ramp.tif", "bad.tif" ]
