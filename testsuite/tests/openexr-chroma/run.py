#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = ["../.."] + sys.path
import runtest

# Start off
command = "echo 'hi' > out.txt"


# ../openexr-images-1.5.0/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = "../../../../openexr-images-1.5.0/Chromaciticies"
# FIXME - we don't currently understand chromaticities

# ../openexr-images-1.5.0/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = "../../../../openexr-images-1.5.0/LuminanceChroma"
#command = command + "; " + runtest.rw_command (imagedir, "CrissyField.exr", path)
#command = command + "; " + runtest.rw_command (imagedir, "Flowers.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Garden.exr", path)
#command = command + "; " + runtest.rw_command (imagedir, "MtTamNorth.exr", path)
#command = command + "; " + runtest.rw_command (imagedir, "StarField.exr", path)
# FIXME -- most of these are broken, we don't read LuminanceChroma images,
#     nor do we currently support subsampled channels


# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
exit (ret)
