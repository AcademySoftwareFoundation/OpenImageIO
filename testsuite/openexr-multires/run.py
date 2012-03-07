#!/usr/bin/python 

# ../openexr-images-1.5.0/MultiResolution:
# Bonita.exr              MirrorPattern.exr       StageEnvCube.exr
# ColorCodedLevels.exr    OrientationCube.exr     StageEnvLatLong.exr
# Kapaa.exr               OrientationLatLong.exr  WavyLinesCube.exr
# KernerEnvCube.exr       PeriodicPattern.exr     WavyLinesLatLong.exr
# KernerEnvLatLong.exr    README                  WavyLinesSphere.exr
imagedir = parent + "/openexr-images-1.5.0/MultiResolution"
files = [ "Bonita.exr", "ColorCodedLevels.exr",
          # FIXME -- we don't know how to deal with RIP-maps -- Kapaa, 
          "KernerEnvCube.exr", "KernerEnvLatLong.exr", "MirrorPattern.exr",
          "OrientationCube.exr", "OrientationLatLong.exr",
          "PeriodicPattern.exr", "StageEnvCube.exr", "StageEnvLatLong.exr",
          "WavyLinesCube.exr", "WavyLinesLatLong.exr", "WavyLinesSphere.exr" ]

for f in files:
    command += rw_command (imagedir, f)
