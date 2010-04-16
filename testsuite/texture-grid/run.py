#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

# A command to run
testtex=""
if (platform.system() == "Windows"):
    testtext = "testtex/Release/testtex"
else:
    testtex = "testtex/testtex"
command = path + "testtex/testtex ../../../oiio-testimages/grid.tx ; "
command = command + path + "idiff/idiff out.exr ref/out.exr > out.txt"

# Outputs to check against references
outputs = [  ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" "out.exr" ]


# boilerplate
sys.path = [".."] + sys.path
import runtest
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
