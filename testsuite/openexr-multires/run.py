#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# ../openexr-images-1.5.0/MultiResolution:
# Bonita.exr              MirrorPattern.exr       StageEnvCube.exr
# ColorCodedLevels.exr    OrientationCube.exr     StageEnvLatLong.exr
# Kapaa.exr               OrientationLatLong.exr  WavyLinesCube.exr
# KernerEnvCube.exr       PeriodicPattern.exr     WavyLinesLatLong.exr
# KernerEnvLatLong.exr    README                  WavyLinesSphere.exr
imagedir = runtest.parent + "/openexr-images-1.5.0/MultiResolution"
command = ""
command = command + runtest.rw_command (imagedir, "Bonita.exr")
command = command + runtest.rw_command (imagedir, "ColorCodedLevels.exr")
#command = command + runtest.rw_command (imagedir, "Kapaa.exr")
command = command + runtest.rw_command (imagedir, "KernerEnvCube.exr")
command = command + runtest.rw_command (imagedir, "KernerEnvLatLong.exr")
command = command + runtest.rw_command (imagedir, "MirrorPattern.exr")
command = command + runtest.rw_command (imagedir, "OrientationCube.exr")
command = command + runtest.rw_command (imagedir, "OrientationLatLong.exr")
command = command + runtest.rw_command (imagedir, "PeriodicPattern.exr")
command = command + runtest.rw_command (imagedir, "StageEnvCube.exr")
command = command + runtest.rw_command (imagedir, "StageEnvLatLong.exr")
command = command + runtest.rw_command (imagedir, "WavyLinesCube.exr")
command = command + runtest.rw_command (imagedir, "WavyLinesLatLong.exr")
command = command + runtest.rw_command (imagedir, "WavyLinesSphere.exr")
# FIXME -- we don't know how to deal with RIP-maps -- Kapaa, 



# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
