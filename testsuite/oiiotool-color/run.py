#!/usr/bin/env python

from __future__ import absolute_import
import os


# Make test pattern with increasing intensity left to right, decreasing
# alpha going down. Carefully done so that the first pixel is 0.0, last
# pixel is 1.0 (correcting for the half pixel offset).
command += oiiotool ("-pattern fill:topleft=0,0,0,1:topright=1,1,1,1:bottomleft=0,0,0,0:bottomright=1,1,1,0 256x256 4 "
                     + " -d uint8 -o greyalpha_linear.tif")


# test --colormap
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --colormap inferno " +
                     "-d uint8 -o colormap-inferno.tif")
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif" +
                     " --colormap .25,.25,.25,0,.5,0,1,0,0 " +
                     "-d uint8 -o colormap-custom.tif")

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
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=0 linear sRGB -o greyalpha_sRGB.tif")
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=0 linear Cineon -o greyalpha_Cineon.tif")

# colorconvert with unpremult
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=1 linear sRGB -o greyalpha_sRGB_un.tif")
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=1 linear Cineon -o greyalpha_Cineon_un.tif")

# test color convert by matrix
command += oiiotool ("--autocc " + "../common/tahoe-tiny.tif"+
                     " "
                     + "--ccmatrix 0.805,0.506,-0.311,0,-0.311,0.805,0.506,0,0.506,-0.311,0.805,0,0,0,0,1 "
                     + "-d uint8 -o tahoe-ccmatrix.tif")

# test various behaviors and misbehaviors related to OCIO configs.
command += oiiotool ("--nostderr --colorconfig missing.ocio -echo 'Nonexistant config'", failureok=True)

#   What happens when we read an OCIOv2 config? In particular, when building
#   against OCIOv1, we should at worst have an error message, not crash with
#   an uncaught exception.
command += oiiotool ("--nostderr --colorconfig ../common/OpenColorIO/ocio-v2_demo.ocio -echo 'OCIOv2 config Ok'", failureok=True)


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
            "rgbfromtga.png",
            "greyalpha_sRGB.tif",
            "greyalpha_Cineon.tif",
            "greyalpha_sRGB_un.tif",
            "greyalpha_Cineon_un.tif",
            "tahoe-ccmatrix.tif",
            "tahoe-sat0.tif",
            "tahoe-sat2.tif",
            "out.txt" ]
