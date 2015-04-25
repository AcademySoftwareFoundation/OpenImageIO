#!/usr/bin/env python 

import os.path

imagedir = parent + "oiio-images"
refdir2 = "../../../../testsuite/oiiotool/ref/"
refdir3 = "../../../../testsuite/oiiotool-composite/ref/"
refdir4 = "../../../../testsuite/oiiotool-fixnan/ref/"
refdir5 = "../../../../testsuite/oiiotool-deep/ref/"
refdir6 = "../../../../testsuite/oiiotool-pattern/ref/"


def checkref (name) :
    if os.path.isfile(refdir2+name) :
        return diff_command (name, refdir2+name)
    elif os.path.isfile(refdir3+name) :
        return diff_command (name, refdir3+name)
    elif os.path.isfile(refdir4+name) :
        return diff_command (name, refdir4+name)
    elif os.path.isfile(refdir5+name) :
        return diff_command (name, refdir5+name)
    elif os.path.isfile(refdir6+name) :
        return diff_command (name, refdir6+name)
    else :
        return diff_command (name, refdir+name)


# Run the script 
command += "python test_imagebufalgo.py > out.txt ;"

# Checkout outputs -- some of the refs are in the oiiotool test dir
for f in [ "black.tif", "filled.tif", "checker.tif",
           "noise-uniform3.tif", "noise-gauss.tif", "noise-salt.tif",
           "chanshuffle.tif", "ch-rgba.exr", "ch-z.exr",
           "chappend-rgbaz.exr",
           "flat.exr", "deepen.exr",
           "crop.tif", "cut.tif", "pasted.tif",
           "rotate90.tif", "rotate180.tif", "rotate270.tif",
           "rotated.tif", "rotated-offcenter.tif",
           "warped.tif",
           "flip.tif", "flop.tif", "flipflop.tif", "reorient1.tif",
           "transpose.tif",
           "cshift.tif", "cadd1.exr", "cadd2.exr", "add.exr", "sub.exr",
           "abs.exr", "absdiff.exr",
           "mul.exr", "cmul1.exr", "cmul2.exr",
           "mad.exr",
           "cpow1.exr", "cpow2.exr",
           "div.exr", "divc1.exr", "divc2.exr",
           "invert.tif",
           "chsum.tif", "grid-clamped.tif",
           "rangecompress.tif", "rangeexpand.tif",
           "resize.tif", "resample.tif",
           "bsplinekernel.exr", "bspline-blur.tif", "tahoe-median.tif",
           "unsharp.tif", "unsharp-median.tif",
           "tahoe-filled.tif",
           "box3.exr",
           "a_over_b.exr",
           "tahoe-small.tx",
           "fft.exr", "ifft.exr", "polar.exr", "complex.exr",
         ] :
    command += checkref (f)


# compare the outputs
outputs = [ "out.txt" ]

