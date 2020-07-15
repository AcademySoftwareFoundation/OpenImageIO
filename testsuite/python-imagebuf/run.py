#!/usr/bin/env python 

# Run the script 
command += pythonbin + " src/test_imagebuf.py > out.txt ;"

# compare the outputs
outputs = [ "out.tif", "outtuple.tif",
            "outarray.tif", "outarrayB.tif", "outarrayH.tif",
            "perchan.exr", "multipart.exr",
            "out.txt" ]

