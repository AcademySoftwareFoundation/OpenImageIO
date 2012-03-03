#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = (runtest.oiio_app ("testtex") + " -res 256 256 --nowarp "
           + runtest.parent + "/oiio-images/miplevels.tx"
           + " -o out.tif ;\n")
command = command + runtest.diff_command ("out.tif", "ref/out.tif",
                                          "--fail 0.0005 --warn 0.0005")

# Outputs to check against references
outputs = [ ]


# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
