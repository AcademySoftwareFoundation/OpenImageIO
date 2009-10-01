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

# ../TGAUTILS:
# CBW8.TGA    CCM8.TGA    CTC16.TGA   CTC24.TGA   CTC32.TGA
# UBW8.TGA    UCM8.TGA    UTC16.TGA   UTC24.TGA   UTC32.TGA
imagedir = "../../../TGAUTILS"
command = command + "; " + runtest.rw_command (imagedir, "CBW8.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "CCM8.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "CTC16.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "CTC24.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "CTC32.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "UBW8.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "UCM8.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "UTC16.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "UTC24.TGA", path)
command = command + "; " + runtest.rw_command (imagedir, "UTC32.TGA", path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
