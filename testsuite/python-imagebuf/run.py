#!/usr/bin/env python 

imagedir = parent + "oiio-images"

# Run the script 
command += "python test_imagebuf.py > out.txt ;"

# compare the outputs
outputs = [ "out.tif", "out.txt" ]

