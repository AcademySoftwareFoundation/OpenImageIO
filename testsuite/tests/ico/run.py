#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

# A command to run
command = path + "iconvert/iconvert ../../../../oiio-testimages/oiio.ico test.ico > out.txt; " + \
path + "idiff/idiff -a ../../../../oiio-testimages/oiio.ico test.ico >> out.txt; " + \
path + "iinfo/iinfo -v -a --hash ../../../../oiio-testimages/oiio.ico test.ico >> out.txt"

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
