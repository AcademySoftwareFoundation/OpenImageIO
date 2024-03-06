#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# ../openexr-images/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Chromaticities"
files = [
    "Rec709.exr", "XYZ.exr"
]
for f in files:
    command += rw_command (imagedir, f)

# ../openexr-images/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/LuminanceChroma"
command += rw_command (imagedir, "Garden.exr")
