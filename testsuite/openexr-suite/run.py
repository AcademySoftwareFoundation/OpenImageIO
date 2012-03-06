#!/usr/bin/python 

# ../openexr-images-1.5.0/DisplayWindow:
# README   t03.exr  t06.exr  t09.exr  t12.exr  t15.exr
# t01.exr  t04.exr  t07.exr  t10.exr  t13.exr  t16.exr
# t02.exr  t05.exr  t08.exr  t11.exr  t14.exr
imagedir = parent + "/openexr-images-1.5.0/DisplayWindow"

# ../openexr-images-1.5.0/ScanLines:
# Blobbies.exr   Desk.exr       StillLife.exr
# Cannon.exr     MtTamWest.exr  Tree.exr
imagedir = parent + "/openexr-images-1.5.0/ScanLines"
files = [ "Desk.exr", "MtTamWest.exr" ]
for f in files:
    command += rw_command (imagedir, f)
command = command + rw_command (imagedir, "Cannon.exr", extraargs="--compression zip")
files = [ "StillLife.exr", "Tree.exr", "Blobbies.exr" ]
for f in files:
    command += rw_command (imagedir, f)
# Cannon must be instructed to use lossless compression
# FIXME - on all: chromaticies, screenWindowCenter, preview?

# ../openexr-images-1.5.0/TestImages:
# AllHalfValues.exr        GrayRampsDiagonal.exr    SquaresSwirls.exr
# BrightRings.exr          GrayRampsHorizontal.exr  WideColorGamut.exr
# BrightRingsNanInf.exr    README                   WideFloatRange.exr
# GammaChart.exr           RgbRampsDiagonal.exr
imagedir = parent + "/openexr-images-1.5.0/TestImages"
files = [ "AllHalfValues.exr", "BrightRings.exr", "BrightRingsNanInf.exr",
          "GammaChart.exr", "GrayRampsDiagonal.exr",
          "GrayRampsHorizontal.exr", "RgbRampsDiagonal.exr",
          "SquaresSwirls.exr", "WideColorGamut.exr", "WideFloatRange.exr" ]
for f in files:
    command += rw_command (imagedir, f)

# ../openexr-images-1.5.0/Tiles:
# GoldenGate.exr  Ocean.exr       Spirals.exr
imagedir = parent + "/openexr-images-1.5.0/Tiles"
files = [ "GoldenGate.exr", "Ocean.exr" ]
for f in files:
    command += rw_command (imagedir, f)
# FIXME - Spirals: per-channel formats, iv >4 chans, chromaticities
