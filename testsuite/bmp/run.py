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

# Start off
hi = "echo hi"
command = hi + "> out.txt"


# ../bmpsuite:
imagedir = "../../../bmpsuite"
command = command + "; " + runtest.rw_command (imagedir, "g01bg.bmp", path, 0)
command = command + runtest.rw_command (imagedir, "g04.bmp", path, 0)
command = command + runtest.rw_command (imagedir, "g08.bmp", path, 0)
command = command + runtest.rw_command (imagedir, "g16bf555.bmp", path, 0)
command = command + runtest.rw_command (imagedir, "g24.bmp", path, 0)
command = command + runtest.rw_command (imagedir, "g32bf.bmp", path, 0)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
