#!/usr/bin/env python 

import math
import OpenImageIO as oiio
from OpenImageIO import ImageBuf, ImageSpec, ImageBufAlgo



def make_constimage (xres, yres, chans=3, format=oiio.UINT8, value=(0,0,0),
                xoffset=0, yoffset=0) :
    spec = ImageSpec (xres,yres,chans,format)
    spec.x = xoffset
    spec.y = yoffset
    b = ImageBuf (spec)
    oiio.ImageBufAlgo.fill (b, value)
    return b


def write (image, filename, format=oiio.UNKNOWN) :
    if not image.has_error :
        image.set_write_format (format)
        image.write (filename)
    if image.has_error :
        print "Error writing", filename, ":", image.geterror()



######################################################################
# main test starts here

try:
    # Some handy images to work with
    gridname = "../../../../../oiio-images/grid.tif"
    grid = ImageBuf (gridname)
    checker = ImageBuf(ImageSpec(256, 256, 3, oiio.UINT8))
    ImageBufAlgo.checker (checker, 8, 8, 8, (0,0,0), (1,1,1))
    gray128 = make_constimage (128, 128, 3, oiio.HALF, (0.5,0.5,0.5))
    gray64 = make_constimage (64, 64, 3, oiio.HALF, (0.5,0.5,0.5))

    # black
    b = ImageBuf (ImageSpec(320,240,3,oiio.UINT8))
    ImageBufAlgo.zero (b)
    write (b, "black.tif")

    # fill (including use of ROI)
    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8));
    ImageBufAlgo.fill (b, (1,0.5,0.5))
    ImageBufAlgo.fill (b, (0,1,0), oiio.ROI(100,180,100,180))
    write (b, "filled.tif")

    # checker
    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8))
    ImageBufAlgo.checker (b, 64, 64, 64, (1,.5,.5), (.5,1,.5), 10, 5)
    write (b, "checker.tif")

    # noise-uniform
    b = ImageBuf (ImageSpec(64,64,3,oiio.UINT8))
    ImageBufAlgo.zero (b)
    ImageBufAlgo.noise (b, "uniform", 0.25, 0.75)
    write (b, "noise-uniform3.tif")

    # noise-gaussian
    b = ImageBuf (ImageSpec(64,64,3,oiio.UINT8))
    ImageBufAlgo.zero (b)
    ImageBufAlgo.noise (b, "gaussian", 0.5, 0.1);
    write (b, "noise-gauss.tif")

    # noise-gaussian
    b = ImageBuf (ImageSpec(64,64,3,oiio.UINT8))
    ImageBufAlgo.zero (b)
    ImageBufAlgo.noise (b, "salt", 1, 0.01);
    write (b, "noise-salt.tif")

    # channels, channel_append
    b = ImageBuf()
    ImageBufAlgo.channels (b, grid, (0.25,2,"G"))
    write (b, "chanshuffle.tif")
    b = ImageBuf()
    ImageBufAlgo.channels (b, ImageBuf("../oiiotool/src/rgbaz.exr"),
                           ("R","G","B","A"))
    write (b, "ch-rgba.exr")
    b = ImageBuf()
    ImageBufAlgo.channels (b, ImageBuf("../oiiotool/src/rgbaz.exr"),
                                ("Z",))
    write (b, "ch-z.exr")
    b = ImageBuf()
    ImageBufAlgo.channel_append (b, ImageBuf("ch-rgba.exr"),
                                 ImageBuf("ch-z.exr"))
    write (b, "chappend-rgbaz.exr")

    # flatten
    b = ImageBuf()
    ImageBufAlgo.flatten (b, ImageBuf("../oiiotool-deep/src/deepalpha.exr"))
    write (b, "flat.exr")

    # deepen
    b = ImageBuf()
    ImageBufAlgo.deepen (b, ImageBuf("../oiiotool-deep/az.exr"))
    write (b, "deepen.exr")

    # crop
    b = ImageBuf()
    ImageBufAlgo.crop (b, grid, oiio.ROI(50,150,200,600))
    write (b, "crop.tif")

    # cut
    b = ImageBuf()
    ImageBufAlgo.cut (b, grid, oiio.ROI(50,150,200,600))
    write (b, "cut.tif")

    # paste
    b = ImageBuf()
    b.copy (checker)
    ImageBufAlgo.paste (b, 150, 75, 0, 0, grid)
    write (b, "pasted.tif")

    # rotate90
    b = ImageBuf()
    ImageBufAlgo.rotate90 (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "rotate90.tif")

    # rotate180
    b = ImageBuf()
    ImageBufAlgo.rotate180 (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "rotate180.tif")

    # rotate270
    b = ImageBuf()
    ImageBufAlgo.rotate270 (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "rotate270.tif")

    # flip
    b = ImageBuf()
    ImageBufAlgo.flip (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "flip.tif")

    # flop
    b = ImageBuf()
    ImageBufAlgo.flop (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "flop.tif")

    # reorient
    b = ImageBuf()
    image_small = ImageBuf()
    ImageBufAlgo.resample (image_small, ImageBuf("../oiiotool/src/image.tif"),  roi=oiio.ROI(0,160,0,120))
    ImageBufAlgo.rotate90 (image_small, image_small)
    image_small.specmod().attribute ("Orientation", 8)
    ImageBufAlgo.reorient (b, image_small)
    write (b, "reorient1.tif")
    image_small = ImageBuf()

    # transpose
    b = ImageBuf()
    ImageBufAlgo.transpose (b, ImageBuf("../oiiotool/src/image.tif"))
    write (b, "transpose.tif")

    # circular_shift
    b = ImageBuf()
    ImageBufAlgo.circular_shift (b, ImageBuf("../oiiotool/src/image.tif"),
                                 100, 50)
    write (b, "cshift.tif")

    # clamp
    b = ImageBuf()
    ImageBufAlgo.resize (b, grid, roi=oiio.ROI(0,500,0,500))
    ImageBufAlgo.clamp (b, b, (0.2,0.2,0.2,0.2), (100,100,0.5,1))
    write (b, "grid-clamped.tif", oiio.UINT8)

    # add
    b = ImageBuf()
    ImageBufAlgo.add (b, gray128, 0.25)
    write (b, "cadd1.exr")
    b = ImageBuf()
    ImageBufAlgo.add (b, gray128, (0, 0.25, -0.25))
    write (b, "cadd2.exr")
    b = ImageBuf()
    ImageBufAlgo.add (b, make_constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                      make_constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "add.exr")

    # sub
    b = ImageBuf()
    ImageBufAlgo.sub (b, make_constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                      make_constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "sub.exr")

    # Test --absdiff and --abs
    # First, make a test image that's 0.5 on the left, -0.5 on the right
    a = ImageBuf (ImageSpec(128,128,3,oiio.HALF))
    ImageBufAlgo.fill (a, (0.5,0.5,0.5))
    ImageBufAlgo.fill (a, (-0.25,-0.25,-0.25), oiio.ROI(0,64,0,128))
    b = ImageBuf()
    ImageBufAlgo.abs (b, a)
    write (b, "abs.exr", oiio.HALF)
    b = ImageBuf()
    ImageBufAlgo.absdiff (b, a, (0.2,0.2,0.2))
    write (b, "absdiff.exr", oiio.HALF)
    a = ImageBuf()

    # mul
    b = ImageBuf()
    ImageBufAlgo.mul (b, gray128, 1.5)
    write (b, "cmul1.exr")
    b = ImageBuf()
    ImageBufAlgo.mul (b, gray128, (1.5,1,0.5))
    write (b, "cmul2.exr")
    b = ImageBuf()
    ImageBufAlgo.mul (b, make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                         make_constimage(64,64,3,oiio.HALF,(1.5,1,0.5)))
    write (b, "mul.exr", oiio.HALF)

    # mad
    b = ImageBuf()
    ImageBufAlgo.mad (b, make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                         make_constimage(64,64,3,oiio.HALF,(1.5,1,0.5)),
                         make_constimage(64,64,3,oiio.HALF,(0.1,0.1,0.1)))
    write (b, "mad.exr", oiio.HALF)
    ImageBufAlgo.mad (b, make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                         (1.5,1,0.5),
                         (0.1,0.1,0.1))
    write (b, "mad2.exr", oiio.HALF)
    ImageBufAlgo.mad (b, make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                         (1.5,1,0.5),
                         make_constimage(64,64,3,oiio.HALF,(0.1,0.1,0.1)))
    write (b, "mad3.exr", oiio.HALF)

    # div
    b = ImageBuf()
    ImageBufAlgo.div (b, gray64, make_constimage (64, 64, 3, oiio.HALF, (2.0,1,0.5)))
    write (b, "div.exr", oiio.HALF)
    b = ImageBuf()
    ImageBufAlgo.div (b, gray64, 2.0)
    write (b, "divc1.exr", oiio.HALF)
    b = ImageBuf()
    ImageBufAlgo.div (b, gray64, (2.0,1,0.5))
    write (b, "divc2.exr", oiio.HALF)

    # invert
    a = ImageBuf ("../oiiotool/src/tahoe-small.tif")
    b = ImageBuf()
    ImageBufAlgo.invert (b, a)
    write (b, "invert.tif", oiio.UINT8)

    # pow
    b = ImageBuf()
    ImageBufAlgo.pow (b, gray128, 2)
    write (b, "cpow1.exr")
    b = ImageBuf()
    ImageBufAlgo.pow (b, gray128, (2,2,1))
    write (b, "cpow2.exr")

    # channel_sum
    b = ImageBuf()
    ImageBufAlgo.channel_sum (b, ImageBuf("../oiiotool/src/tahoe-small.tif"),
                              (.2126,.7152,.0722))
    write (b, "chsum.tif", oiio.UINT8)

    # color_map
    b = ImageBuf()
    ImageBufAlgo.color_map (b, ImageBuf("../oiiotool/src/tahoe-tiny.tif"),
                           -1, "inferno")
    write (b, "colormap-inferno.tif", oiio.UINT8)
    b = ImageBuf()
    ImageBufAlgo.color_map (b, ImageBuf("../oiiotool/src/tahoe-tiny.tif"),
                           -1, 3, 3, (.25,.25,.25,0,.5,0,1,0,0))
    write (b, "colormap-custom.tif", oiio.UINT8)

    # premult/unpremult
    b = make_constimage(100,100,4,oiio.FLOAT,(.1,.1,.1,1))
    ImageBufAlgo.fill (b, (.2,.2,.2,.5), oiio.ROI(50,80,50,80))
    ImageBufAlgo.unpremult (b, b)
    write (b, "unpremult.tif")
    ImageBufAlgo.premult (b, b)
    write (b, "premult.tif")

    b = ImageBuf ("../oiiotool/src/tahoe-small.tif")
    ImageBufAlgo.rangecompress (b, b)
    write (b, "rangecompress.tif", oiio.UINT8)
    ImageBufAlgo.rangeexpand (b, b)
    write (b, "rangeexpand.tif", oiio.UINT8)

    # FIXME - colorconvert, ociolook need tests

    # computePixelStats
    b = ImageBuf ("../oiiotool/src/tahoe-small.tif")
    stats = oiio.PixelStats()
    ImageBufAlgo.computePixelStats (b, stats)
    print ("Stats for tahoe-small.tif:")
    print "  min         = ", stats.min
    print "  max         = ", stats.max
    print "  avg         = ", stats.avg
    print "  stddev      = ", stats.stddev
    print "  nancount    = ", stats.nancount
    print "  infcount    = ", stats.infcount
    print "  finitecount = ", stats.finitecount

    compresults = oiio.CompareResults()
    ImageBufAlgo.compare (ImageBuf("flip.tif"), ImageBuf("flop.tif"),
                          1.0e-6, 1.0e-6, compresults)
    print "Comparison: of flip.tif and flop.tif"
    print "  mean = %.5g" % compresults.meanerror
    print "  rms  = %.5g" % compresults.rms_error
    print "  PSNR = %.5g" % compresults.PSNR
    print "  max  = %.5g" % compresults.maxerror
    print "  max @", (compresults.maxx, compresults.maxy, compresults.maxz, compresults.maxc)
    print "  warns", compresults.nwarn, "fails", compresults.nfail

    # compare_Yee,
    # isConstantColor, isConstantChannel

    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8));
    ImageBufAlgo.fill (b, (1,0.5,0.5))
    r = ImageBufAlgo.isConstantColor (b)
    print "isConstantColor on pink image is (%.5g %.5g %.5g)" % r
    r = ImageBufAlgo.isConstantColor (checker)
    print "isConstantColor on checker is ", r

    b = ImageBuf("cmul1.exr")
    print "Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b)
    b = ImageBuf("cmul2.exr")
    print "Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b)


    # color_count, color_range_check

    # nonzero_region
    b = make_constimage (256,256,3,oiio.UINT8,(0,0,0))
    ImageBufAlgo.fill (b, (0,0,0))
    ImageBufAlgo.fill (b, (0,1,0), oiio.ROI(100,180,100,180))
    print "Nonzero region is: ", ImageBufAlgo.nonzero_region(b)

    # resize
    b = ImageBuf()
    ImageBufAlgo.resize (b, grid, roi=oiio.ROI(0,256,0,256))
    write (b, "resize.tif")

    # resample
    b = ImageBuf()
    ImageBufAlgo.resample (b, grid, roi=oiio.ROI(0,128,0,128))
    write (b, "resample.tif")

    # warp
    b = ImageBuf()
    Mwarp = (0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 128, -53.01933, 1)
    ImageBufAlgo.warp (b, ImageBuf("resize.tif"), Mwarp)
    write (b, "warped.tif")

    # rotate
    b = ImageBuf()
    ImageBufAlgo.rotate (b, ImageBuf("resize.tif"), math.radians(45.0))
    write (b, "rotated.tif")
    b = ImageBuf()
    ImageBufAlgo.rotate (b, ImageBuf("resize.tif"), math.radians(45.0), 50.0, 50.0)
    write (b, "rotated-offcenter.tif")

    # make_kernel
    bsplinekernel = ImageBuf()
    ImageBufAlgo.make_kernel (bsplinekernel, "bspline", 15, 15)
    write (bsplinekernel, "bsplinekernel.exr")

    # convolve -- test with bspline blur
    b = ImageBuf()
    ImageBufAlgo.convolve (b, ImageBuf("../oiiotool/src/tahoe-small.tif"),
                           bsplinekernel)
    write (b, "bspline-blur.tif", oiio.UINT8)

    # median filter
    b = ImageBuf()
    ImageBufAlgo.median_filter (b, ImageBuf("../oiiotool/src/tahoe-small.tif"),
                                5, 5)
    write (b, "tahoe-median.tif", oiio.UINT8)

    # Dilate/erode
    b = ImageBuf()
    undilated = ImageBuf("../oiiotool/src/morphsource.tif")
    ImageBufAlgo.dilate (b, undilated, 3, 3)
    write (b, "dilate.tif", oiio.UINT8)
    b = ImageBuf()
    ImageBufAlgo.erode (b, undilated, 3, 3)
    write (b, "erode.tif", oiio.UINT8)
    undilated = None

    # unsharp_mask
    b = ImageBuf()
    ImageBufAlgo.unsharp_mask (b, ImageBuf("../oiiotool/src/tahoe-small.tif"),
                               "gaussian", 3.0, 1.0, 0.0)
    write (b, "unsharp.tif", oiio.UINT8)

    # unsharp_mark with median filter
    b = ImageBuf()
    ImageBufAlgo.unsharp_mask (b, ImageBuf("../oiiotool/src/tahoe-small.tif"),
                               "median", 3.0, 1.0, 0.0)
    write (b, "unsharp-median.tif", oiio.UINT8)

    # laplacian
    b = ImageBuf()
    ImageBufAlgo.laplacian (b, ImageBuf("../oiiotool/src/tahoe-tiny.tif"))
    write (b, "tahoe-laplacian.tif", oiio.UINT8)

    # computePixelHashSHA1
    print ("SHA-1 of bsplinekernel.exr is: " + 
           ImageBufAlgo.computePixelHashSHA1(bsplinekernel))

    # fft, ifft
    fft = ImageBuf()
    blue = ImageBuf()
    ImageBufAlgo.channels (blue, ImageBuf("../oiiotool/src/tahoe-tiny.tif"),
                           (2,))
    ImageBufAlgo.fft (fft, blue)
    write (fft, "fft.exr", oiio.FLOAT)
    inv = ImageBuf()
    ImageBufAlgo.ifft (inv, fft)
    b = ImageBuf()
    ImageBufAlgo.channels (b, inv, (0,))
    write (b, "ifft.exr", oiio.FLOAT)
    inv.clear()
    fft.clear()

    fft = ImageBuf("fft.exr")
    polar = ImageBuf()
    ImageBufAlgo.complex_to_polar (polar, fft)
    b = ImageBuf()
    ImageBufAlgo.polar_to_complex (b, polar)
    write (polar, "polar.exr", oiio.FLOAT)
    write (b, "unpolar.exr", oiio.FLOAT)
    fft.clear()
    polar.clear()

    # fixNonFinite
    bad = ImageBuf ("../oiiotool-fixnan/src/bad.exr")
    b = ImageBuf()
    ImageBufAlgo.fixNonFinite (b, bad, oiio.NONFINITE_BOX3)
    write (b, "box3.exr")
    bad.clear()

    # fillholes_pushpull
    b = ImageBuf()
    ImageBufAlgo.fillholes_pushpull (b, ImageBuf("../oiiotool/ref/hole.tif"))
    write (b, "tahoe-filled.tif", oiio.UINT8)

    # over
    b = ImageBuf()
    ImageBufAlgo.over (b, ImageBuf("../oiiotool-composite/src/a.exr"),
                       ImageBuf("../oiiotool-composite/src/b.exr"))
    write (b, "a_over_b.exr")

    # FIXME - no test for zover (not in oiio-composite either)

    b = make_constimage (320, 240, 3, oiio.FLOAT)
    ImageBufAlgo.render_text (b, 25, 50, "Hello, world",
                              16, "DroidSerif", (1,1,1))
    ImageBufAlgo.render_text (b, 50, 120, "Go Big Red!",
                              42, "", (1,0,0))
    write (b, "text.tif", oiio.UINT8)

    b = make_constimage (320, 240, 3, oiio.FLOAT)
    broi = b.roi
    textsize = ImageBufAlgo.text_size ("Centered", 40)
    if textsize.defined :
        x = broi.xbegin + broi.width/2  - (textsize.xbegin + textsize.width/2)
        y = broi.ybegin + broi.height/2 - (textsize.ybegin + textsize.height/2)
        ImageBufAlgo.render_text (b, x, y, "Centered", 40)
    write (b, "textcentered.tif", oiio.UINT8)

    # histogram, histogram_draw,

    # make_texture
    ImageBufAlgo.make_texture (oiio.MakeTxTexture,
                               ImageBuf("../oiiotool/src/tahoe-small.tif"),
                               "tahoe-small.tx")

    # capture_image - no test

    print "Done."
except Exception as detail:
    print "Unknown exception:", detail

