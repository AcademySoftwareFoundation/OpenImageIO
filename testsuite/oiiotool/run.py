#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = [".."] + sys.path
import runtest

# A command to run
command = "echo hi > out.txt ; "
command = command + path + runtest.oiio_app ("oiiotool") + " ../../../oiio-images/grid.tif --resize 256x256 -o resize.tif >> out.txt ; "
command = command + path + runtest.oiio_app ("idiff") + " resize.tif ref/resize.tif >> out.txt ;"

# To add more tests, just append more lines here, like this:
#  command = command + path + runtest.oiio_app ("oiiotool") + " ../../../oiio-images/grid.tif OPTIONS -o feature.tif >> out.txt ; "
#  command = command + path + runtest.oiio_app ("idiff") + " feature.tif ref/feature.tif >> out.txt ;"
# and also add the new 'feature.tif' (or whatever you call it) to the outputs
# list, below.


# Outputs to check against references
outputs = [ "resize.tif" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" ]

print "Running this command:\n" + command + "\n"

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
