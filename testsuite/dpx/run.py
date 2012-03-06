#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# List of images to test
imagedir = runtest.parent + "/oiio-images"
files = [ "dpx_nuke_10bits_rgb.dpx", "dpx_nuke_16bits_rgba.dpx" ]

# Run the tests
command = ""
for f in files:
    command = command + " ;\n" + runtest.rw_command (imagedir, f)

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
