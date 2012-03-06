#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

imagedir = "ref/"
files = [ "norle-8.sgi", "rle-8.sgi", "norle-16.sgi", "rle-16.sgi" ]
command = ""
for f in files:
    command = command + runtest.rw_command (imagedir, f)

ret = runtest.runtest (command)
sys.exit (ret)
