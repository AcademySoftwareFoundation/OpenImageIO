#!/usr/bin/python 

# ../openexr-images/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = parent + "/openexr-images/Chromaciticies"
# FIXME - we don't currently understand chromaticities

# ../openexr-images/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = parent + "/openexr-images/LuminanceChroma"
#command += rw_command (imagedir, "CrissyField.exr", extraargs="--compression zip")
#command += rw_command (imagedir, "Flowers.exr", extraargs="--compression zip")
command += rw_command (imagedir, "Garden.exr")
#command += rw_command (imagedir, "MtTamNorth.exr")
#command += rw_command (imagedir, "StarField.exr")
# FIXME -- most of these are broken, we don't read LuminanceChroma images,
#     nor do we currently support subsampled channels
