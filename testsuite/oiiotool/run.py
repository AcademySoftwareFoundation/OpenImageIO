#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = ""
command = command + (runtest.oiio_app ("oiiotool") + " " 
                     + runtest.parent + "/oiio-images/grid.tif"
                     + " --resize 256x256 -o resize.tif >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "resize.tif" ]

#print "Running this command:\n" + command + "\n"

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
