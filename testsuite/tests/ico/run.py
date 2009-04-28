#!/usr/bin/python 

# A command to run
command = "iconvert ../../../../oiio-testimages/oiio.ico test.ico > out.txt; \
idiff -a ../../../../oiio-testimages/oiio.ico test.ico >> out.txt; \
iinfo -v -a --md5 ../../../../oiio-testimages/oiio.ico test.ico >> out.txt"

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "test.ico" ]


# boilerplate
import sys
sys.path = ["../.."] + sys.path
import runtest
ret = runtest.runtest (command, outputs, cleanfiles)
exit (ret)
