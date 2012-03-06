#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# Start off
imagedir = runtest.parent + "/oiio-images"

# Run the tests
command = runtest.rw_command (imagedir, "oiio.ico")

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
