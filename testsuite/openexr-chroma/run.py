#!/usr/bin/python 

# ../openexr-images-1.5.0/Chromaticities:
# README         Rec709.exr     Rec709_YC.exr  XYZ.exr        XYZ_YC.exr
imagedir = parent + "/openexr-images-1.5.0/Chromaciticies"
# FIXME - we don't currently understand chromaticities

# ../openexr-images-1.5.0/LuminanceChroma:
# CrissyField.exr  Garden.exr       StarField.exr
# Flowers.exr      MtTamNorth.exr
imagedir = parent + "/openexr-images-1.5.0/LuminanceChroma"
#command += rw_command (imagedir, "CrissyField.exr", extraargs="--compression zip")
#command += rw_command (imagedir, "Flowers.exr", extraargs="--compression zip")
command += rw_command (imagedir, "Garden.exr")
#command += rw_command (imagedir, "MtTamNorth.exr")
#command += rw_command (imagedir, "StarField.exr")
# FIXME -- most of these are broken, we don't read LuminanceChroma images,
#     nor do we currently support subsampled channels
