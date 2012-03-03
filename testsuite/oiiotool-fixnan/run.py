#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = ""
command = command + (runtest.oiio_app ("oiiotool") + 
                     " bad.exr --fixnan black -o black.exr >> out.txt ;\n")
command = command + (runtest.oiio_app ("oiiotool") + 
                     " bad.exr --fixnan box3 -o box3.exr >> out.txt ;\n")
command = command + runtest.info_command ("bad.exr", "--stats")
command = command + runtest.info_command ("black.exr", "--stats")
command = command + runtest.info_command ("box3.exr", "--stats")

# Outputs to check against references
outputs = [ "black.exr", "box3.exr", "out.txt" ]

print "Running this command:\n" + command + "\n"

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
