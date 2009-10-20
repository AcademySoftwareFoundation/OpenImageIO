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


# ../openexr-images-1.5.0/DisplayWindow:
# README   t03.exr  t06.exr  t09.exr  t12.exr  t15.exr
# t01.exr  t04.exr  t07.exr  t10.exr  t13.exr  t16.exr
# t02.exr  t05.exr  t08.exr  t11.exr  t14.exr
imagedir = "../../../openexr-images-1.5.0/DisplayWindow"

# ../openexr-images-1.5.0/ScanLines:
# Blobbies.exr   Desk.exr       StillLife.exr
# Cannon.exr     MtTamWest.exr  Tree.exr
imagedir = "../../../openexr-images-1.5.0/ScanLines"
#command = command + "; " + runtest.rw_command (imagedir, "Blobbies.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Desk.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Cannon.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Desk.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "MtTamWest.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "StillLife.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Tree.exr", path)
# FIXME - Blobbies: per-channel formats, iv >4 chans
# FIXME - on all: chromaticies, screenWindowCenter, preview?

# ../openexr-images-1.5.0/TestImages:
# AllHalfValues.exr        GrayRampsDiagonal.exr    SquaresSwirls.exr
# BrightRings.exr          GrayRampsHorizontal.exr  WideColorGamut.exr
# BrightRingsNanInf.exr    README                   WideFloatRange.exr
# GammaChart.exr           RgbRampsDiagonal.exr
imagedir = "../../../openexr-images-1.5.0/TestImages"
command = command + "; " + runtest.rw_command (imagedir, "AllHalfValues.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "BrightRings.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "BrightRingsNanInf.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "GammaChart.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "GrayRampsDiagonal.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "GrayRampsHorizontal.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "RgbRampsDiagonal.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "SquaresSwirls.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "WideColorGamut.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "WideFloatRange.exr", path)

# ../openexr-images-1.5.0/Tiles:
# GoldenGate.exr  Ocean.exr       Spirals.exr
imagedir = "../../../openexr-images-1.5.0/Tiles"
command = command + "; " + runtest.rw_command (imagedir, "GoldenGate.exr", path)
command = command + "; " + runtest.rw_command (imagedir, "Ocean.exr", path)
#command = command + "; " + runtest.rw_command (imagedir, "Spirals.exr", path)
# FIXME - Spirals: per-channel formats, iv >4 chans, chromaticities


# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "AllHalfValues.exr", "BrightRings.exr", "BrightRingsNanInf.exr",
               "Cannon.exr", "Desk.exr", "GammaChart.exr", "GoldenGate.exr",
               "GrayRampsDiagonal.exr", "GrayRampsHorizontal.exr",
               "MtTamWest.exr", "Ocean.exr", "RgbRampsDiagonal.exr",
               "SquaresSwirls.exr", "StillLife.exr", "Tree.exr",
               "WideColorGamut.exr", "WideFloatRange.exr"]

# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
