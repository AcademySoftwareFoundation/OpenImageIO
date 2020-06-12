#!/usr/bin/env python

# ../openexr-images/MultiResolution:
# Bonita.exr              MirrorPattern.exr       StageEnvCube.exr
# ColorCodedLevels.exr    OrientationCube.exr     StageEnvLatLong.exr
# Kapaa.exr               OrientationLatLong.exr  WavyLinesCube.exr
# KernerEnvCube.exr       PeriodicPattern.exr     WavyLinesLatLong.exr
# KernerEnvLatLong.exr    README                  WavyLinesSphere.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/MultiResolution"
files = [ "Bonita.exr", "ColorCodedLevels.exr",
          # FIXME -- we don't know how to deal with RIP-maps -- Kapaa, 
          "KernerEnvCube.exr", "KernerEnvLatLong.exr", "MirrorPattern.exr",
          "OrientationCube.exr", "OrientationLatLong.exr",
          "PeriodicPattern.exr", "StageEnvCube.exr", "StageEnvLatLong.exr",
          "WavyLinesCube.exr", "WavyLinesLatLong.exr", "WavyLinesSphere.exr" ]
for f in files:
    command += rw_command (imagedir, f)


# ../openexr-images/MultiView
# Adjuster.exr  Balls.exr  Fog.exr  Impact.exr  LosPadres.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/MultiView"
files = [ "Adjuster.exr", "Balls.exr", "Fog.exr", "Impact.exr", "LosPadres.exr" ]
for f in files:
    command += rw_command (imagedir, f)
