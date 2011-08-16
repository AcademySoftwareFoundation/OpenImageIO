#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = [".."] + sys.path
import runtest

# A command to run
command = path + runtest.oiio_app ("oiiotool") + " ../../../oiio-images/grid.tif --resize 256x256 -o out.tif ; "
command = command + path + runtest.oiio_app ("idiff") + " out.tif ref/out.tif > out.txt"

# Outputs to check against references
outputs = [ "out.tif" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
