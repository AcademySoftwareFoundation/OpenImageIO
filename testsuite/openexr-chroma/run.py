#!/usr/bin/env python

# ../openexr-images/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Chromaticities"
files = [
    "Rec709.exr", "XYZ.exr"
]
for f in files:
    command += rw_command (imagedir, f)
# FIXME - we don't currently subsampled images (Rec709_YC.exr and XYZ_YC.exr)

# ../openexr-images/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/LuminanceChroma"
#command += rw_command (imagedir, "CrissyField.exr", extraargs="--compression zip")
#command += rw_command (imagedir, "Flowers.exr", extraargs="--compression zip")
command += rw_command (imagedir, "Garden.exr")
#command += rw_command (imagedir, "MtTamNorth.exr")
#command += rw_command (imagedir, "StarField.exr")
# FIXME -- most of these are broken, we don't read LuminanceChroma images,
#     nor do we currently support subsampled channels
