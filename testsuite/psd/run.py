#!/usr/bin/python

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

imagedir = runtest.parent + "/oiio-images/"
files = [ "psd_123.psd", "psd_123_nomaxcompat.psd", "psd_bitmap.psd", \
          "psd_indexed_trans.psd", "psd_rgb_8.psd", "psd_rgb_16.psd", \
          "psd_rgb_32.psd", "psd_rgba_8.psd" ]
command = ""
for f in files:
    command = command + runtest.info_command (imagedir + f)

# boilerplate
ret = runtest.runtest (command)
sys.exit (ret)
