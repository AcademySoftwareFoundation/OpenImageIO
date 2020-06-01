#!/usr/bin/env python

# ../openexr-images/DisplayWindow:
# README   t03.exr  t06.exr  t09.exr  t12.exr  t15.exr
# t01.exr  t04.exr  t07.exr  t10.exr  t13.exr  t16.exr
# t02.exr  t05.exr  t08.exr  t11.exr  t14.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/DisplayWindow"

# ../openexr-images/ScanLines:
# Blobbies.exr   Desk.exr       StillLife.exr
# Cannon.exr     MtTamWest.exr  Tree.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/ScanLines"
files = [ "Desk.exr", "MtTamWest.exr" ]
for f in files:
    command += rw_command (imagedir, f)
command = command + rw_command (imagedir, "Cannon.exr", extraargs="--compression zip")
files = [ "StillLife.exr", "Tree.exr", "Blobbies.exr" ]
for f in files:
    command += rw_command (imagedir, f)
# Cannon must be instructed to use lossless compression
# FIXME - on all: screenWindowCenter, preview?

# ../openexr-images/TestImages:
# AllHalfValues.exr        GrayRampsDiagonal.exr    SquaresSwirls.exr
# BrightRings.exr          GrayRampsHorizontal.exr  WideColorGamut.exr
# BrightRingsNanInf.exr    README                   WideFloatRange.exr
# GammaChart.exr           RgbRampsDiagonal.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/TestImages"
files = [ "AllHalfValues.exr", "BrightRings.exr", "BrightRingsNanInf.exr",
          "GammaChart.exr", "GrayRampsDiagonal.exr",
          "GrayRampsHorizontal.exr", "RgbRampsDiagonal.exr",
          "SquaresSwirls.exr", "WideColorGamut.exr", "WideFloatRange.exr" ]
for f in files:
    command += rw_command (imagedir, f)

# ../openexr-images/Tiles:
# GoldenGate.exr  Ocean.exr       Spirals.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Tiles"
files = [ "GoldenGate.exr", "Ocean.exr", "Spirals.exr" ]
for f in files:
    command += rw_command (imagedir, f)


# Check a complicated channel and layer ordering example
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Beachball"
files = [ "singlepart.0001.exr" ]
for f in files:
    command += rw_command (imagedir, f)
