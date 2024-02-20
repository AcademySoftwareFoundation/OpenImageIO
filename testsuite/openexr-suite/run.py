#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# ../openexr-images/ScanLines:
# Blobbies.exr     Cannon.exr  MtTamWest.exr     StillLife.exr
# CandleGlass.exr  Desk.exr    PrismsLenses.exr  Tree.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/ScanLines"
files = [ "Desk.exr", "MtTamWest.exr" ]
files = [
    "Blobbies.exr", "CandleGlass.exr", "Cannon.exr", "Desk.exr",
    "MtTamWest.exr", "PrismsLenses.exr", "StillLife.exr", "Tree.exr"
]
for f in files:
    command += rw_command (imagedir, f)


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
