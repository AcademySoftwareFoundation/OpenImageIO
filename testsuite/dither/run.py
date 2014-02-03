#!/usr/bin/env python 

# Run the script 
command += "python make_ramp.py > out.txt ;"

# compare the outputs
files = [ "ramp.tif" ]
for f in files :
    command += diff_command (f, "ref/"+f)

