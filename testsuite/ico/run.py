#!/usr/bin/python 
# -*- coding: utf-8 -*-

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
command = "echo hi> out.txt"

imagedir = "../../../oiio-testimages"

# Run the tests
command = command + "; " + runtest.rw_command (imagedir, "oiio.ico", path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "oiio.ico" ]

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
