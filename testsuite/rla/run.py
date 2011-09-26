#!/usr/bin/python 
# -*- coding: utf-8 -*-

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

imagedir = "../../../oiio-images"

# List of images to test
files = [ "ginsu_a_nc10.rla", "ginsu_a_ncf.rla", "ginsu_rgba_nc8.rla", \
          "ginsu_rgb_nc16.rla", "imgmake_rgba_nc10.rla", "ginsu_a_nc16.rla", \
          "ginsu_rgba_nc10.rla", "ginsu_rgba_ncf.rla", "ginsu_rgb_nc8.rla", \
          "imgmake_rgba_nc16.rla", "ginsu_a_nc8.rla", "ginsu_rgba_nc16.rla", \
          "ginsu_rgb_nc10.rla", "ginsu_rgb_ncf.rla", "imgmake_rgba_nc8.rla" ]

# Run the tests
for f in files:
    command = command + "; " + runtest.rw_command (imagedir, f, path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = files

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
