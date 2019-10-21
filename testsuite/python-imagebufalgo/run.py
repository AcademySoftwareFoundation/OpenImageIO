#!/usr/bin/env python

refdirlist = [
    OIIO_TESTSUITE_ROOT + "/oiiotool/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-color/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-copy/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-composite/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-fixnan/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-deep/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-pattern/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-text/ref/",
    OIIO_TESTSUITE_ROOT + "/oiiotool-xform/ref/",
    refdir
]

# Run the script
command += pythonbin + " src/test_imagebufalgo.py > out.txt ;"

# Checkout outputs -- some of the refs are in the oiiotool test dir
outputs = ["black.tif", "filled.tif", "checker.tif",
           "noise-uniform3.tif", "noise-gauss.tif", "noise-salt.tif",
           "chanshuffle.tif", "ch-rgba.exr", "ch-z.exr",
           "chappend-rgbaz.exr",
           "flat.exr", "deepen.exr",
           "crop.tif", "cut.tif", "pasted.tif",
           "rotate90.tif", "rotate180.tif", "rotate270.tif",
           "rotated.tif", "rotated-offcenter.tif",
           "warped.tif",
           "flip.tif", "flop.tif", "reorient1.tif",
           "transpose.tif",
           "cshift.tif", "cadd1.exr", "cadd2.exr", "add.exr", "sub.exr",
           "abs.exr", "absdiff.exr",
           "mul.exr", "cmul1.exr", "cmul2.exr",
           "mad.exr", "mad2.exr", "mad3.exr",
           "cpow1.exr", "cpow2.exr",
           "div.exr", "divc1.exr", "divc2.exr",
           "invert.tif",
           "chsum.tif", "colormap-inferno.tif", "colormap-custom.tif",
           "grid-clamped.tif",
           "rangecompress.tif", "rangeexpand.tif",
           "contrast-stretch.tif", "contrast-shrink.tif",
           "contrast-sigmoid5.tif",
           "resize.tif", "resample.tif", "fit.tif",
           "bsplinekernel.exr", "bspline-blur.tif", "tahoe-median.tif",
           "dilate.tif", "erode.tif",
           "unsharp.tif", "unsharp-median.tif", "tahoe-laplacian.tif",
           "fft.exr", "ifft.exr", "polar.exr", "unpolar.exr",
           "tahoe-filled.tif",
           "box3.exr",
           "a_over_b.exr",
           "tahoe-small.tx",
           "text.tif", "textcentered.tif",
           "out.txt"
         ]
    # command += checkref (f)

