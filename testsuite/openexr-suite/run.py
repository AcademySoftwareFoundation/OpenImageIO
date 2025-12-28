#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1"

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

# Check writing overscan and negative range
command += oiiotool("--create 64x64-16-16 3 -d half -o negoverscan.exr")
command += info_command("negoverscan.exr", safematch=True)

# Check ACES Container output for relaxed mode
#
# Valid ACES Container
command += oiiotool("--create 4x4 3 -d half --compression none -sattrib openexr:ACESContainerPolicy relaxed -o relaxed-out.exr")
command += oiiotool("relaxed-out.exr --echo \"acesImageContainerFlag for {TOP.filename} is ({TOP[acesImageContainerFlag]})\"", failureok=True) # should give 1

# Invalid channel name set
command += oiiotool("--create 4x4 3 -d half --compression none --ch left.R=R,G,B -sattrib openexr:ACESContainerPolicy relaxed -o fail.exr")
command += oiiotool("fail.exr --echo \"acesImageContainerFlag for {TOP.filename} is ({TOP[acesImageContainerFlag]})\"", failureok=True) # should be empty

# Invalid compression
command += oiiotool("--create 4x4 3 -d half --compression zip -sattrib openexr:ACESContainerPolicy relaxed -o fail.exr")
command += oiiotool("fail.exr --echo \"acesImageContainerFlag for {TOP.filename} is ({TOP[acesImageContainerFlag]})\"", failureok=True) # should be empty

# Invalid data type
command += oiiotool("--create 4x4 3 -d float --compression none -sattrib openexr:ACESContainerPolicy relaxed -o fail.exr")
command += oiiotool("fail.exr --echo \"acesImageContainerFlag for {TOP.filename} is ({TOP[acesImageContainerFlag]})\"", failureok=True) # should be empty

# Check ACES Container output for strict mode
#
# Valid ACES Container
command += oiiotool("--create 4x4 3 -d half --compression none -sattrib openexr:ACESContainerPolicy strict -o strict-out.exr")
command += info_command("strict-out.exr", safematch=True)

# Invalid channel name set
command += oiiotool("--create 4x4 3 -d half --compression none --ch left.R=R,G,B -sattrib openexr:ACESContainerPolicy strict -o strict-fail.exr", failureok=True)

# Invalid compression
command += oiiotool("--create 4x4 3 -d half --compression zip -sattrib openexr:ACESContainerPolicy strict -o strict-fail.exr", failureok=True)

# Invalid data type
command += oiiotool("--create 4x4 3 -d float --compression none -sattrib openexr:ACESContainerPolicy strict -o strict-fail.exr", failureok=True)

# Check color interop ID output
command += oiiotool("--create 4x4 3 --attrib oiio:ColorSpace scene_linear -o color_interop_id_scene_linear.exr")
command += info_command("color_interop_id_scene_linear.exr", safematch=True)
command += oiiotool("--create 4x4 3 --attrib oiio:ColorSpace lin_adobergb_scene -o color_interop_id_linear_adobergb.exr")
command += info_command("color_interop_id_linear_adobergb.exr", safematch=True)
command += oiiotool("--create 4x4 3 --attrib oiio:ColorSpace unknown_interop_id -o color_interop_id_unknown.exr")
command += info_command("color_interop_id_unknown.exr", safematch=True)
