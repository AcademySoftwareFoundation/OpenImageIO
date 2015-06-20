#!/usr/bin/env python 

import os.path

imagedir = parent + "oiio-images"
refdir2 = "../oiiotool/ref/"
refdir3 = "../oiiotool-composite/ref/"
refdir4 = "../oiiotool-fixnan/ref/"
refdir5 = "../oiiotool-deep/ref/"
failthresh = 0.004
failpercent = 0.2


def checkref (name) :
    if os.path.isfile(refdir2+name) :
        return diff_command (name, refdir2+name)
    elif os.path.isfile(refdir3+name) :
        return diff_command (name, refdir3+name)
    elif os.path.isfile(refdir4+name) :
        return diff_command (name, refdir4+name)
    elif os.path.isfile(refdir5+name) :
        return diff_command (name, refdir5+name)
    else :
        return diff_command (name, refdir+name)


# Run the script 
command += "python test_imagebufalgo.py > out.txt ;"

# Checkout outputs -- some of the refs are in the oiiotool test dir
for f in [ "black.tif", "filled.tif", "checker.tif",
           "chanshuffle.tif", "ch-rgba.exr", "ch-z.exr",
           "chappend-rgbaz.exr", "flat.exr",
           "crop.tif", "cut.tif", "pasted.tif",
           "rotate90.tif", "rotate180.tif", "rotate270.tif",
           "rotated.tif", "rotated-offcenter.tif",
           "warped.tif",
           "flip.tif", "flop.tif", "flipflop.tif", "reorient1.tif",
           "transpose.tif",
           "cshift.tif", "cadd1.exr", "cadd2.exr", "add.exr", "sub.exr",
           "cmul1.exr", "cmul2.exr", "cpow1.exr", "cpow2.exr",
           "chsum.tif", "grid-clamped.tif",
           "rangecompress.tif", "rangeexpand.tif",
           "resize.tif", "resample.tif",
           "bsplinekernel.exr", "bspline-blur.tif", "tahoe-median.tif",
           "unsharp.tif", "unsharp-median.tif",
           "fft.exr", "ifft.exr", "polar.exr", "unpolar.exr",
           "tahoe-filled.tif",
           "box3.exr",
           "a_over_b.exr",
           "tahoe-small.tx",
         ] :
    command += checkref (f)


# compare the outputs
outputs = [ "out.txt" ]

