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
command = command + path + runtest.oiio_app ("oiiotool") + " bad.exr --fixnan black -o black.exr >> out.txt ; "
command = command + path + runtest.oiio_app ("oiiotool") + " bad.exr --fixnan box3 -o box3.exr >> out.txt ; "
command = command + path + runtest.oiio_app ("idiff") + " black.exr ref/black.exr >> out.txt ;"
command = command + path + runtest.oiio_app ("idiff") + " box3.exr ref/box3.exr >> out.txt ;"
command = command + path + runtest.oiio_app ("oiiotool") + " -v --stats bad.exr black.exr box3.exr >> out.txt ;"

# Outputs to check against references
outputs = [ "out.txt", "black.exr", "box3.exr" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]

print "Running this command:\n" + command + "\n"

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
