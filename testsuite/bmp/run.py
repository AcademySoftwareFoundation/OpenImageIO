#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest


# bmpsuite:
imagedir = runtest.parent + "/bmpsuite"
command =           runtest.rw_command (imagedir, "g01bg.bmp")
command = command + runtest.rw_command (imagedir, "g04.bmp")
command = command + runtest.rw_command (imagedir, "g08.bmp")
command = command + runtest.rw_command (imagedir, "g16bf555.bmp")
command = command + runtest.rw_command (imagedir, "g24.bmp")
command = command + runtest.rw_command (imagedir, "g32bf.bmp")

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
