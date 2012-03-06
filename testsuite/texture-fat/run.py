#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = runtest.testtex_command ("horizgrid.tx", " --scalest 1 4")

# Outputs to check against references
outputs = [ "out.exr" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
