#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


from __future__ import absolute_import
import os

# print("ociover =", ociover)

# Make test pattern with increasing intensity left to right, decreasing
# alpha going down. Carefully done so that the first pixel is 0.0, last
# pixel is 1.0 (correcting for the half pixel offset).
command += oiiotool ("-pattern fill:topleft=0,0,0,1:topright=1,1,1,1:bottomleft=0,0,0,0:bottomright=1,1,1,0 256x256 4 "
                     + " -d uint8 -o greyalpha_lin_srgb.tif")
command += oiiotool ("-pattern fill:topleft=0,0,0:topright=1,1,1:bottomleft=0,0,0:bottomright=1,1,1 256x256 3 "
                     + " -d uint8 -o grey_lin_srgb.tif")


# test --colormap
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --colormap inferno " +
                     "-d uint8 -o colormap-inferno.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --colormap .25,.25,.25,0,.5,0,1,0,0 " +
                     "-d uint8 -o colormap-custom.tif")

colormaps = [ "magma", "inferno", "plasma", "viridis", "turbo", "blue-red", "spectrum", "heat" ]
for c in colormaps :
    command += oiiotool ("--pattern fill:left=0,0,0:right=1,1,1 64x64 3" +
                         " --colormap " + c +
                         " -d uint8 -o cmap-" + c + ".tif")

# test unpremult/premult
command += oiiotool ("--pattern constant:color=.1,.1,.1,1 100x100 4 " 
            + " --fill:color=.2,.2,.2,.5 30x30+50+50 "
            + " -d half -o premulttarget.exr")
command += oiiotool ("premulttarget.exr --unpremult -o unpremult.exr")
command += oiiotool ("unpremult.exr --premult -o premult.exr")

# test --no-autopremult on a TGA file thet needs it.
command += oiiotool ("--no-autopremult src/rgba.tga --ch R,G,B -o rgbfromtga.png")

# test --contrast
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " -contrast:black=0.1:white=0.75 -d uint8 -o contrast-stretch.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " -contrast:min=0.1:max=0.75 -d uint8 -o contrast-shrink.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " -contrast:black=1:white=0 -d uint8 -o contrast-inverse.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " -contrast:black=1,1,.25:white=1,1,0.25 -d uint8 -o contrast-threshold.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " -contrast:scontrast=5 -d uint8 -o contrast-sigmoid5.tif")

# test --saturate
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --saturate 0 -d uint8 -o tahoe-sat0.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --saturate 2 -d uint8 -o tahoe-sat2.tif")



#
# Test basic color transformation / OCIO functionality
#

# colorconvert without unpremult
if float(ociover) >= 2.2 :
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=0 lin_srgb sRGB -o greyalpha_sRGB.tif")
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=1 lin_srgb sRGB -o greyalpha_sRGB_un.tif")
    command += oiiotool ("grey_lin_srgb.tif --colorconvert:unpremult=0 lin_srgb sRGB -o grey_sRGB.tif")
    command += oiiotool ("grey_lin_srgb.tif --colorconvert:unpremult=1 lin_srgb sRGB -o grey_sRGB_un.tif")
else:
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=0 linear sRGB -o greyalpha_sRGB.tif")
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=0 linear Cineon -o greyalpha_Cineon.tif")
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=1 linear sRGB -o greyalpha_sRGB_un.tif")
    command += oiiotool ("greyalpha_lin_srgb.tif --colorconvert:unpremult=1 linear Cineon -o greyalpha_Cineon_un.tif")
    command += oiiotool ("grey_lin_srgb.tif --colorconvert:unpremult=0 linear sRGB -o grey_sRGB.tif")
    command += oiiotool ("grey_lin_srgb.tif --colorconvert:unpremult=1 linear sRGB -o grey_sRGB_un.tif")
 
# test color convert by matrix
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif"+
                     " "
                     + "--ccmatrix 0.805,0.506,-0.311,0,-0.311,0.805,0.506,0,0.506,-0.311,0.805,0,0,0,0,1 "
                     + "-d uint8 -o tahoe-ccmatrix.tif")

# Apply a display
if float(ociover) >= 2.2 :
    command += oiiotool ("greyalpha_lin_srgb.tif --iscolorspace lin_srgb --ociodisplay \"sRGB - Display\" Un-tone-mapped -o display-sRGB.tif")
else :
    command += oiiotool ("greyalpha_lin_srgb.tif --ociodisplay default sRGB -o display-sRGB.tif")

# Applying a look
if float(ociover) >= 2.2 :
    command += oiiotool ("--autocc ../common/tahoe-tiny.tif --ociolook \"ACES 1.3 Reference Gamut Compression\" -o look-default.tif")

# TODO: should test applying a file transform

# test various behaviors and misbehaviors related to OCIO configs.
command += oiiotool ("--nostderr --colorconfig missing.ocio -echo \"Nonexistent config\"", failureok=True)

#   What happens when we read an OCIOv2 config? In particular, when building
#   against OCIOv1, we should at worst have an error message, not crash with
#   an uncaught exception.
command += oiiotool ("--nostderr --colorconfig ../common/OpenColorIO/ocio-v2_demo.ocio -echo \"OCIOv2 config Ok\"", failureok=True)


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "colormap-inferno.tif", "colormap-custom.tif",
            "unpremult.exr", "premult.exr",
            "contrast-stretch.tif",
            "contrast-shrink.tif",
            "contrast-inverse.tif",
            "contrast-threshold.tif",
            "contrast-sigmoid5.tif",
            "display-sRGB.tif",
            "rgbfromtga.png",
            "greyalpha_sRGB.tif",
            "greyalpha_sRGB_un.tif",
            "grey_sRGB.tif",
            "grey_sRGB_un.tif",
            "tahoe-ccmatrix.tif",
            "tahoe-sat0.tif",
            "tahoe-sat2.tif"
    ]
for c in colormaps :
    outputs += [ "cmap-" + c + ".tif" ]
if float(ociover) >= 2.2 :
    outputs += [ "look-default.tif" ]
else :
    outputs += [ 
            "greyalpha_Cineon.tif",
            "greyalpha_Cineon_un.tif",
    ]
outputs += [ "out.txt" ]
