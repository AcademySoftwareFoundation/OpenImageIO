#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = runtest.testtex_command ("missing.tx", "--missing 1 0 0 --res 8 8 missing.tx")

# Outputs to check against references
outputs = [ "out.exr" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
