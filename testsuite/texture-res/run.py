#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

# A command to run
command = path + "testtex/testtex -res 256 256 --nowarp ../../../oiio-testimages/miplevels.tx -o out.tif; "
command = command + path + "idiff/idiff out.tif ref/out.tif > out.txt"

# Outputs to check against references
outputs = [  ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" "out.tif" ]


# boilerplate
import sys
sys.path = [".."] + sys.path
import runtest
ret = runtest.runtest (command, outputs, cleanfiles)
exit (ret)
