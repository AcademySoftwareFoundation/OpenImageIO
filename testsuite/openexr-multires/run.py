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
hi = "echo hi"
command = hi + "> out.txt"


# ../openexr-images-1.5.0/MultiResolution:
# Bonita.exr              MirrorPattern.exr       StageEnvCube.exr
# ColorCodedLevels.exr    OrientationCube.exr     StageEnvLatLong.exr
# Kapaa.exr               OrientationLatLong.exr  WavyLinesCube.exr
# KernerEnvCube.exr       PeriodicPattern.exr     WavyLinesLatLong.exr
# KernerEnvLatLong.exr    README                  WavyLinesSphere.exr
imagedir = "../../../openexr-images-1.5.0/MultiResolution"
command = command + "; " + runtest.rw_command (imagedir, "Bonita.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "ColorCodedLevels.exr", path)
#command = command + "; " + runtest.rw_command (imagedir, "Kapaa.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "KernerEnvCube.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "KernerEnvLatLong.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "MirrorPattern.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "OrientationCube.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "OrientationLatLong.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "PeriodicPattern.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "StageEnvCube.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "StageEnvLatLong.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "WavyLinesCube.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "WavyLinesLatLong.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "WavyLinesSphere.exr", path)
# FIXME -- we don't know how to deal with RIP-maps -- Kapaa, 



# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
