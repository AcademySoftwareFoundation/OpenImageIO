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

# List of images to test
files = [ "dpx_nuke_10bits_rgb.dpx", "dpx_nuke_16bits_rgba.dpx" ]

# Run the tests
for f in files:
    command = command + "; " + runtest.rw_command (imagedir, f, path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = files

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
