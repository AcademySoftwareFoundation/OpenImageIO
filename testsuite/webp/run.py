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
command = path + runtest.oiio_app("iinfo") + " -v ../../../webp-images/1.webp > out.txt;"
command += path + runtest.oiio_app("iinfo") + " -v ../../../webp-images/2.webp >> out.txt;"
command += path + runtest.oiio_app("iinfo") + " -v ../../../webp-images/3.webp >> out.txt;"
command += path + runtest.oiio_app("iinfo") + " -v ../../../webp-images/4.webp >> out.txt;"
command += path + runtest.oiio_app("iinfo") + " -v ../../../webp-images/5.webp >> out.txt;"

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
