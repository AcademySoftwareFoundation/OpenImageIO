#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = (runtest.oiio_app("maketx") + " --filter lanczos3 "
           + runtest.parent + "/oiio-images/grid-overscan.exr"
           + " -o grid-overscan.exr ;\n")
command = command + runtest.testtex_command ("grid-overscan.exr", "--wrap black")

# Outputs to check against references
outputs = [ "out.exr" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
