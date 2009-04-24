#!/usr/bin/python 

# A command to run
command = "iconvert ../../../../oiio-testimages/oiio.ico test.ico > out.txt && \
idiff ../../../../oiio-testimages/oiio.ico test.ico >> out.txt && \
iinfo -v -a ../../../../oiio-testimages/oiio.ico test.ico >> out.txt"

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.ico" ]


# boilerplate
import sys
sys.path = ["../.."] + sys.path
import runtest
ret = runtest.runtest (command, outputs, cleanfiles)
exit (ret)
