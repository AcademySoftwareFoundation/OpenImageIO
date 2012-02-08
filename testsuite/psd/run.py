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

# Start off
command = "echo hi> out.txt"

imagedir = "../../../oiio-images/"

# List of images to test
files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd", \
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd", \
          "psd_rgb_32.psd", "psd_rgba_8.psd" ]

# Run the tests
for f in files:
    command = command + "; " + path + runtest.oiio_app ("iinfo") + " -a -v " + imagedir + f + " >> out.txt"

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = files

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
