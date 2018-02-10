#!/usr/bin/env python

# Make test pattern with increasing intensity left to right, decreasing
# alpha going down. Carefully done so that the first pixel is 0.0, last
# pixel is 1.0 (correcting for the half pixel offset).
command += oiiotool ("-pattern fill:topleft=0,0,0,1:topright=1,1,1,1:bottomleft=0,0,0,0:bottomright=1,1,1,0 256x256 4 "
                     + " -d uint8 -o greyalpha_linear.tif")


# test --colormap
command += oiiotool ("--autocc ../oiiotool/src/tahoe-tiny.tif --colormap inferno "
            + "-d uint8 -o colormap-inferno.tif")
command += oiiotool ("--autocc ../oiiotool/src/tahoe-tiny.tif --colormap .25,.25,.25,0,.5,0,1,0,0 "
            + "-d uint8 -o colormap-custom.tif")

# test unpremult/premult
command += oiiotool ("--pattern constant:color=.1,.1,.1,1 100x100 4 " 
            + " --fill:color=.2,.2,.2,.5 30x30+50+50 "
            + " -d half -o premulttarget.exr")
command += oiiotool ("premulttarget.exr --unpremult -o unpremult.exr")
command += oiiotool ("unpremult.exr --premult -o premult.exr")

# test --no-autopremult on a TGA file thet needs it.
command += oiiotool ("--no-autopremult src/rgba.tga --ch R,G,B -o rgbfromtga.png")


#
# Test basic color transformation / OCIO functionality
#

# colorconvert without unpremult
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=0 linear sRGB -o greyalpha_sRGB.tif")
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=0 linear Cineon -o greyalpha_Cineon.tif")

# colorconvert with unpremult
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=1 linear sRGB -o greyalpha_sRGB_un.tif")
command += oiiotool ("greyalpha_linear.tif --colorconvert:unpremult=1 linear Cineon -o greyalpha_Cineon_un.tif")


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "colormap-inferno.tif", "colormap-custom.tif",
            "unpremult.exr", "premult.exr",
            "rgbfromtga.png",
            "greyalpha_sRGB.tif",
            "greyalpha_Cineon.tif",
            "greyalpha_sRGB_un.tif",
            "greyalpha_Cineon_un.tif",
            "out.txt" ]
