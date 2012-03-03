#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = runtest.info_command (runtest.parent + "/oiio-images/tahoe-gps.jpg")

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
