#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest


imagedir = runtest.parent + "/webp-images/"
files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
command = ""
for f in files:
    command = command + runtest.info_command (imagedir + f)

ret = runtest.runtest (command)
sys.exit (ret)
