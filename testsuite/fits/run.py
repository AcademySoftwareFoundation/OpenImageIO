#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# Start off
command = ""

# ../fits-image/pg93:
# tst0001.fits to tst0014.fits
imagedir = runtest.parent + "/fits-images/pg93"
command = command + ";\n" + runtest.rw_command (imagedir, "tst0001.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0002.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0003.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0005.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0006.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0007.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0008.fits")
#command = command + ";\n" + runtest.rw_command (imagedir, "tst0009.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "tst0013.fits")

imagedir = runtest.parent + "/fits-images/ftt4b"
command = command + ";\n" + runtest.rw_command (imagedir, "file001.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "file002.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "file003.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "file009.fits")
command = command + ";\n" + runtest.rw_command (imagedir, "file012.fits")

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
