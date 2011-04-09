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
command = path + runtest.oiio_app ("testtex") + " -fill 0.05 --res 128 128 --nowarp gray.png ; "
command = command + path + runtest.oiio_app ("idiff") + " out.exr ref/out.exr > out.txt"

# Outputs to check against references
outputs = [  ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" "out.exr" "postage.exr" ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
