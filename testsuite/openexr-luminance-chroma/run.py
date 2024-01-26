#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# ../openexr-images/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Chromaticities"
files = [
    "Rec709_YC.exr", "XYZ_YC.exr"
]
for f in files:
    command += rw_command (imagedir, f)

# ../openexr-images/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/LuminanceChroma"
# Request a lossless compression method for the first two images. Otherwise,
# the b44 compression from the input image carries over to the output image and
# causes pretty large compression artefacts.
command += rw_command (imagedir, "CrissyField.exr", extraargs="--compression zip")
command += rw_command (imagedir, "Flowers.exr", extraargs="--compression zip")
command += rw_command (imagedir, "MtTamNorth.exr")
command += rw_command (imagedir, "StarField.exr")
