#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# Start off
command = ""

imagedir = runtest.parent + "/TGAUTILS"
files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA" ]
command = ""
for f in files:
    command = command + runtest.rw_command (imagedir, f)

ret = runtest.runtest (command)
sys.exit (ret)
