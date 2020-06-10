#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import math, os
import OpenImageIO as oiio
from OpenImageIO import ImageBuf, ImageSpec, ImageBufAlgo, ROI


OIIO_TESTSUITE_ROOT = os.getenv('OIIO_TESTSUITE_ROOT', '')
OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR', '')

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
        image.write (filename, format)
    if image.has_error :
        print ("Error writing", filename, ":", image.geterror())

def dumpimg (image, fmt="{:.3f}", msg="") :
    spec = image.spec()
    print (msg, end="")
    for y in range(spec.y, spec.y+spec.height) :
        for x in range(spec.x, spec.x+spec.width) :
            p = image.getpixel (x, y)
            print ("[", end="")
            for c in range(spec.nchannels) :
                print (fmt.format(p[c]), end=" ")
            print ("] ", end="")
        print ("")



######################################################################
# main test starts here

try:
    # Some handy images to work with
    gridname = os.path.join(OIIO_TESTSUITE_IMAGEDIR, "grid.tif")
    grid = ImageBuf (gridname)
    checker = ImageBuf(ImageSpec(256, 256, 3, oiio.UINT8))
    ImageBufAlgo.checker (checker, 8, 8, 8, (0,0,0), (1,1,1))
    gray128 = make_constimage (128, 128, 3, oiio.HALF, (0.5,0.5,0.5))
    gray64 = make_constimage (64, 64, 3, oiio.HALF, (0.5,0.5,0.5))
    tahoetiny = ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-tiny.tif")

    # black
    # b = ImageBuf (ImageSpec(320,240,3,oiio.UINT8))
    b = ImageBufAlgo.zero (roi=oiio.ROI(0,320,0,240,0,1,0,3))
    write (b, "black.tif", oiio.UINT8)

    # fill (including use of ROI)
    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8));
    ImageBufAlgo.fill (b, (1,0.5,0.5))
    ImageBufAlgo.fill (b, (0,1,0), oiio.ROI(100,180,100,180))
    write (b, "filled.tif", oiio.UINT8)

    # checker
    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8))
    ImageBufAlgo.checker (b, 64, 64, 64, (1,.5,.5), (.5,1,.5), 10, 5)
    write (b, "checker.tif", oiio.UINT8)

    # noise-uniform
    b = ImageBufAlgo.noise ("uniform", 0.25, 0.75, roi=ROI(0,64,0,64,0,1,0,3))
    write (b, "noise-uniform3.tif", oiio.UINT8)

    # noise-gaussian
    b = ImageBufAlgo.noise ("gaussian", 0.5, 0.1, roi=ROI(0,64,0,64,0,1,0,3));
    write (b, "noise-gauss.tif", oiio.UINT8)

    # noise-gaussian
    b = ImageBufAlgo.noise ("salt", 1, 0.01, roi=ROI(0,64,0,64,0,1,0,3));
    write (b, "noise-salt.tif", oiio.UINT8)

    # channels, channel_append
    b = ImageBufAlgo.channels (grid, (0.25,2,"G"))
    write (b, "chanshuffle.tif")
    b = ImageBufAlgo.channels (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/rgbaz.exr"),
                               ("R","G","B","A"))
    write (b, "ch-rgba.exr")
    b = ImageBufAlgo.channels (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/rgbaz.exr"), ("Z",))
    write (b, "ch-z.exr")
    b = ImageBufAlgo.channel_append (ImageBuf("ch-rgba.exr"),
                                     ImageBuf("ch-z.exr"))
    write (b, "chappend-rgbaz.exr")

    # flatten
    b = ImageBufAlgo.flatten (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool-deep/src/deepalpha.exr"))
    write (b, "flat.exr")

    # deepen
    b = ImageBufAlgo.deepen (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool-deep/src/az.exr"))
    write (b, "deepen.exr")

    # crop
    b = ImageBufAlgo.crop (grid, oiio.ROI(50,150,200,600))
    write (b, "crop.tif")

    # cut
    b = ImageBufAlgo.cut (grid, oiio.ROI(50,150,200,600))
    write (b, "cut.tif")

    # paste
    b = ImageBuf()
    b.copy (checker)
    ImageBufAlgo.paste (b, 150, 75, 0, 0, grid)
    write (b, "pasted.tif")

    # rotate90
    b = ImageBufAlgo.rotate90 (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "rotate90.tif")

    # rotate180
    b = ImageBufAlgo.rotate180 (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "rotate180.tif")

    # rotate270
    b = ImageBufAlgo.rotate270 (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "rotate270.tif")

    # flip
    b = ImageBufAlgo.flip (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "flip.tif")

    # flop
    b = ImageBufAlgo.flop (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "flop.tif")

    # reorient
    image_small = ImageBuf()
    ImageBufAlgo.resample (image_small, ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"),  roi=oiio.ROI(0,160,0,120))
    image_small = ImageBufAlgo.rotate90 (image_small)
    image_small.specmod().attribute ("Orientation", 8)
    b = ImageBufAlgo.reorient (image_small)
    write (b, "reorient1.tif")
    image_small = ImageBuf()

    # transpose
    b = ImageBufAlgo.transpose (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"))
    write (b, "transpose.tif")

    # circular_shift
    b = ImageBufAlgo.circular_shift (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/image.tif"), 100, 50)
    write (b, "cshift.tif")

    # clamp
    b = ImageBufAlgo.resize (grid, roi=oiio.ROI(0,500,0,500))
    b = ImageBufAlgo.clamp (b, (0.2,0.2,0.2,0.2), (100,100,0.5,1))
    write (b, "grid-clamped.tif", oiio.UINT8)

    # add
    b = ImageBufAlgo.add (gray128, 0.25)
    write (b, "cadd1.exr")
    b = ImageBufAlgo.add (gray128, (0, 0.25, -0.25))
    write (b, "cadd2.exr")
    b = ImageBufAlgo.add (make_constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                          make_constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "add.exr")

    # sub
    b = ImageBufAlgo.sub (make_constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                          make_constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "sub.exr")

    # Test --absdiff and --abs
    # First, make a test image that's 0.5 on the left, -0.5 on the right
    a = ImageBuf (ImageSpec(128,128,3,oiio.HALF))
    ImageBufAlgo.fill (a, (0.5,0.5,0.5))
    ImageBufAlgo.fill (a, (-0.25,-0.25,-0.25), oiio.ROI(0,64,0,128))
    b = ImageBufAlgo.abs (a)
    write (b, "abs.exr", oiio.HALF)
    b = ImageBufAlgo.absdiff (a, (0.2,0.2,0.2))
    write (b, "absdiff.exr", oiio.HALF)
    a = ImageBuf()

    # mul
    b = ImageBufAlgo.mul (gray128, 1.5)
    write (b, "cmul1.exr")
    b = ImageBufAlgo.mul (gray128, (1.5,1,0.5))
    write (b, "cmul2.exr")
    b = ImageBufAlgo.mul (make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                          make_constimage(64,64,3,oiio.HALF,(1.5,1,0.5)))
    write (b, "mul.exr", oiio.HALF)

    # mad
    b = ImageBufAlgo.mad (make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                          make_constimage(64,64,3,oiio.HALF,(1.5,1,0.5)),
                          make_constimage(64,64,3,oiio.HALF,(0.1,0.1,0.1)))
    write (b, "mad.exr", oiio.HALF)
    b = ImageBufAlgo.mad (make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                          (1.5,1,0.5),
                          (0.1,0.1,0.1))
    write (b, "mad2.exr", oiio.HALF)
    b = ImageBufAlgo.mad (make_constimage(64,64,3,oiio.HALF,(.5,.5,.5)),
                          (1.5,1,0.5),
                          make_constimage(64,64,3,oiio.HALF,(0.1,0.1,0.1)))
    write (b, "mad3.exr", oiio.HALF)

    # div
    b = ImageBufAlgo.div (gray64, make_constimage (64, 64, 3, oiio.HALF, (2.0,1,0.5)))
    write (b, "div.exr", oiio.HALF)
    b = ImageBufAlgo.div (gray64, 2.0)
    write (b, "divc1.exr", oiio.HALF)
    b = ImageBufAlgo.div (gray64, (2.0,1,0.5))
    write (b, "divc2.exr", oiio.HALF)

    # invert
    a = ImageBuf (OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif")
    b = ImageBufAlgo.invert (a)
    write (b, "invert.tif", oiio.UINT8)

    # pow
    b = ImageBufAlgo.pow (gray128, 2)
    write (b, "cpow1.exr")
    b = ImageBufAlgo.pow (gray128, (2,2,1))
    write (b, "cpow2.exr")

    # channel_sum
    b = ImageBufAlgo.channel_sum (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"),
                                  (.2126,.7152,.0722))
    write (b, "chsum.tif", oiio.UINT8)

    # color_map
    b = ImageBufAlgo.color_map (tahoetiny, -1, "inferno")
    write (b, "colormap-inferno.tif", oiio.UINT8)
    b = ImageBufAlgo.color_map (tahoetiny, -1, 3, 3, (.25,.25,.25,0,.5,0,1,0,0))
    write (b, "colormap-custom.tif", oiio.UINT8)

    # premult/unpremult
    b = make_constimage(100,100,4,oiio.FLOAT,(.1,.1,.1,1))
    ImageBufAlgo.fill (b, (.2,.2,.2,.5), oiio.ROI(50,80,50,80))
    b = ImageBufAlgo.unpremult (b)
    write (b, "unpremult.tif")
    b = ImageBufAlgo.premult (b)
    write (b, "premult.tif")

    b = ImageBufAlgo.contrast_remap (tahoetiny, black=0.1, white=0.75)
    write (b, "contrast-stretch.tif")
    b = ImageBufAlgo.contrast_remap (tahoetiny, min=0.1, max=0.75)
    write (b, "contrast-shrink.tif")
    b = ImageBufAlgo.contrast_remap (tahoetiny, scontrast=5.0)
    write (b, "contrast-sigmoid5.tif")

    b = ImageBuf (OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif")
    b = ImageBufAlgo.rangecompress (b)
    write (b, "rangecompress.tif", oiio.UINT8)
    b = ImageBufAlgo.rangeexpand (b)
    write (b, "rangeexpand.tif", oiio.UINT8)

    # FIXME - colorconvert, ociolook need tests
    print ("\nTesting color conversions:")
    b = make_constimage (2,2,4,oiio.FLOAT,(0,0,0,1))
    b.setpixel(1, 0, (.25,.25,.25,1))
    b.setpixel(0, 1, (.5,.5,.5,1))
    b.setpixel(1, 1, (1,1,1,1))
    dumpimg (b, msg="linear src=")
    r = ImageBufAlgo.colorconvert(b, "Linear", "sRGB")
    dumpimg (r, msg="to srgb =")
    r = ImageBufAlgo.colorconvert(r, "sRGB", "Linear")
    dumpimg (r, msg="back to linear =")
    # Just to test, make a matrix that halves red, doubles green,
    # adds 0.1 to blue.
    M = ( 0.5, 0, 0,   0,
          0,   2, 0,   0,
          0,   0, 1,   0,
          0,   0, 0.1, 1)
    r = ImageBufAlgo.colormatrixtransform (b, M)
    dumpimg (r, msg="after *M =")

    # computePixelStats
    b = ImageBuf (OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif")
    stats = ImageBufAlgo.computePixelStats (b)
    print ("Stats for tahoe-small.tif:")
    print ("  min         = ", stats.min)
    print ("  max         = ", stats.max)
    print ("  avg         = ", stats.avg)
    print ("  stddev      = ", stats.stddev)
    print ("  nancount    = ", stats.nancount)
    print ("  infcount    = ", stats.infcount)
    print ("  finitecount = ", stats.finitecount)

    compresults = ImageBufAlgo.compare (ImageBuf("flip.tif"), ImageBuf("flop.tif"),
                                        1.0e-6, 1.0e-6)
    print ("Comparison: of flip.tif and flop.tif")
    print ("  mean = %.5g" % compresults.meanerror)
    print ("  rms  = %.5g" % compresults.rms_error)
    print ("  PSNR = %.5g" % compresults.PSNR)
    print ("  max  = %.5g" % compresults.maxerror)
    print ("  max @", (compresults.maxx, compresults.maxy, compresults.maxz, compresults.maxc))
    print ("  warns", compresults.nwarn, "fails", compresults.nfail)

    # compare_Yee,
    # isConstantColor, isConstantChannel

    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8));
    ImageBufAlgo.fill (b, (1,0.5,0.5))
    r = ImageBufAlgo.isConstantColor (b)
    print ("isConstantColor on pink image is (%.5g %.5g %.5g)" % r)
    r = ImageBufAlgo.isConstantColor (checker)
    print ("isConstantColor on checker is ", r)

    b = ImageBuf("cmul1.exr")
    print ("Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b))
    b = ImageBuf("cmul2.exr")
    print ("Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b))


    # color_count

    b = ImageBufAlgo.fill (top=(0,0,0), bottom=(1,1,1), roi=ROI(0,4,0,4,0,1,0,3))
    counts = ImageBufAlgo.color_range_check (b, low=0.25, high=(0.5,0.5,0.5))
    print ('color range counts = ', counts)

    # nonzero_region
    b = make_constimage (256,256,3,oiio.UINT8,(0,0,0))
    ImageBufAlgo.fill (b, (0,0,0))
    ImageBufAlgo.fill (b, (0,1,0), oiio.ROI(100,180,100,180))
    print ("Nonzero region is: ", ImageBufAlgo.nonzero_region(b))

    # resize
    b = ImageBufAlgo.resize (grid, roi=oiio.ROI(0,256,0,256))
    write (b, "resize.tif")

    # resample
    b = ImageBufAlgo.resample (grid, roi=oiio.ROI(0,128,0,128))
    write (b, "resample.tif")

    # fit
    b = ImageBufAlgo.fit (grid, roi=oiio.ROI(0,360,0,240))
    write (b, "fit.tif")

    # warp
    Mwarp = (0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 128, -53.01933, 1)
    b = ImageBufAlgo.warp (ImageBuf("resize.tif"), Mwarp)
    write (b, "warped.tif")

    # rotate
    b = ImageBufAlgo.rotate (ImageBuf("resize.tif"), math.radians(45.0))
    write (b, "rotated.tif")
    b = ImageBufAlgo.rotate (ImageBuf("resize.tif"), math.radians(45.0), 50.0, 50.0)
    write (b, "rotated-offcenter.tif")

    # make_kernel
    bsplinekernel = ImageBufAlgo.make_kernel ("bspline", 15, 15)
    write (bsplinekernel, "bsplinekernel.exr")

    # convolve -- test with bspline blur
    b = ImageBufAlgo.convolve (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"),
                               bsplinekernel)
    write (b, "bspline-blur.tif", oiio.UINT8)

    # median filter
    b = ImageBufAlgo.median_filter (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"), 5, 5)
    write (b, "tahoe-median.tif", oiio.UINT8)

    # Dilate/erode
    undilated = ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/morphsource.tif")
    b = ImageBufAlgo.dilate (undilated, 3, 3)
    write (b, "dilate.tif", oiio.UINT8)
    b = ImageBufAlgo.erode (undilated, 3, 3)
    write (b, "erode.tif", oiio.UINT8)
    undilated = None

    # unsharp_mask
    b = ImageBufAlgo.unsharp_mask (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"),
                                   "gaussian", 3.0, 1.0, 0.0)
    write (b, "unsharp.tif", oiio.UINT8)

    # unsharp_mark with median filter
    b = ImageBufAlgo.unsharp_mask (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"),
                                   "median", 3.0, 1.0, 0.0)
    write (b, "unsharp-median.tif", oiio.UINT8)

    # laplacian
    b = ImageBufAlgo.laplacian (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-tiny.tif"))
    write (b, "tahoe-laplacian.tif", oiio.UINT8)

    # computePixelHashSHA1
    print ("SHA-1 of bsplinekernel.exr is: " +
           ImageBufAlgo.computePixelHashSHA1(bsplinekernel))

    # fft, ifft
    blue = ImageBufAlgo.channels (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-tiny.tif"), (2,))
    fft = ImageBufAlgo.fft (blue)
    write (fft, "fft.exr", oiio.FLOAT)
    inv = ImageBufAlgo.ifft (fft)
    b = ImageBufAlgo.channels (inv, (0,))
    write (b, "ifft.exr", oiio.FLOAT)
    inv.clear()
    fft.clear()

    fft = ImageBuf("fft.exr")
    polar = ImageBufAlgo.complex_to_polar (fft)
    b = ImageBufAlgo.polar_to_complex (polar)
    write (polar, "polar.exr", oiio.FLOAT)
    write (b, "unpolar.exr", oiio.FLOAT)
    fft.clear()
    polar.clear()

    # fixNonFinite
    bad = ImageBuf (OIIO_TESTSUITE_ROOT+"/oiiotool-fixnan/src/bad.exr")
    b = ImageBufAlgo.fixNonFinite (bad, oiio.NONFINITE_BOX3)
    write (b, "box3.exr")
    bad.clear()

    # fillholes_pushpull
    b = ImageBufAlgo.fillholes_pushpull (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/ref/hole.tif"))
    write (b, "tahoe-filled.tif", oiio.UINT8)

    # over
    b = ImageBufAlgo.over (ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool-composite/src/a.exr"),
                           ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool-composite/src/b.exr"))
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
        x = broi.xbegin + broi.width//2  - (textsize.xbegin + textsize.width//2)
        y = broi.ybegin + broi.height//2 - (textsize.ybegin + textsize.height//2)
        ImageBufAlgo.render_text (b, x, y, "Centered", 40)
    write (b, "textcentered.tif", oiio.UINT8)

    # histogram, histogram_draw,
    b = make_constimage (100, 100, 3, oiio.UINT8, (.1, .2, .3))
    Rhist = ImageBufAlgo.histogram (b, channel=0, bins=4)
    Ghist = ImageBufAlgo.histogram (b, channel=1, bins=4)
    Bhist = ImageBufAlgo.histogram (b, channel=2, bins=4)
    print ("R hist: ", Rhist)
    print ("G hist: ", Ghist)
    print ("B hist: ", Bhist)

    # make_texture
    ImageBufAlgo.make_texture (oiio.MakeTxTexture,
                               ImageBuf(OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-small.tif"),
                               "tahoe-small.tx")

    # capture_image - no test

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

