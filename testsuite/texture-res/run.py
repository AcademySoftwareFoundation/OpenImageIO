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
command = path + runtest.oiio_app ("testtex") + " -res 256 256 --nowarp ../../../oiio-testimages/miplevels.tx -o out.tif; "
command = command + path + runtest.oiio_app ("idiff") + " --fail 0.0005 --warn 0.0005 out.tif ref/out.tif > out.txt"

# Outputs to check against references
outputs = [  ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" "out.tif" ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
