#!/usr/bin/env python 

import OpenImageIO as oiio
from OpenImageIO import ImageBuf, ImageSpec, ImageBufAlgo



def constimage (xres, yres, chans=3, format=oiio.UINT8, value=(0,0,0),
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
        print "Error writing", filename, ":", b.geterror()



######################################################################
# main test starts here

try:
    # Some handy images to work with
    gridname = "../../../../../oiio-images/grid.tif"
    grid = ImageBuf (gridname)
    checker = ImageBuf(ImageSpec(256, 256, 3, oiio.UINT8))
    ImageBufAlgo.checker (checker, 8, 8, 8, (0,0,0), (1,1,1))
    gray128 = constimage (128, 128, 3, oiio.HALF, (0.5,0.5,0.5))

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
    ImageBufAlgo.flatten (b, ImageBuf("../oiiotool/src/deepalpha.exr"))
    write (b, "flat.exr")

    # crop
    b = ImageBuf()
    ImageBufAlgo.crop (b, grid, oiio.ROI(50,150,200,600))
    write (b, "crop.tif")

    # paste
    b = ImageBuf()
    b.copy (checker)
    ImageBufAlgo.paste (b, 150, 75, 0, 0, grid)
    write (b, "pasted.tif")

    # flip
    b = ImageBuf()
    ImageBufAlgo.flip (b, ImageBuf("../oiiotool/image.tif"))
    write (b, "flip.tif")

    # flop
    b = ImageBuf()
    ImageBufAlgo.flop (b, ImageBuf("../oiiotool/image.tif"))
    write (b, "flop.tif")

    # flipflop
    b = ImageBuf()
    ImageBufAlgo.flipflop (b, ImageBuf("../oiiotool/image.tif"))
    write (b, "flipflop.tif")

    # transpose
    b = ImageBuf()
    ImageBufAlgo.transpose (b, ImageBuf("../oiiotool/image.tif"))
    write (b, "transpose.tif")

    # circular_shift
    b = ImageBuf()
    ImageBufAlgo.circular_shift (b, ImageBuf("../oiiotool/image.tif"),
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
    ImageBufAlgo.add (b, constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                      constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "add.exr")

    # sub
    b = ImageBuf()
    ImageBufAlgo.sub (b, constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
                      constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    write (b, "sub.exr")

    # mul
    b = ImageBuf()
    ImageBufAlgo.mul (b, gray128, 1.5)
    write (b, "cmul1.exr")
    b = ImageBuf()
    ImageBufAlgo.mul (b, gray128, (1.5,1,0.5))
    write (b, "cmul2.exr")
    # FIXME -- image multiplication; it's not in testsuite/oiiotool either
    # b = ImageBuf()
    # ImageBufAlgo.mul (b, constimage(64,64,3,oiio.HALF,(.1,.2,.3)),
    #                        constimage(64,64,3,oiio.HALF,(.1,.1,.1),20,20))
    # write (b, "mul.exr")

    # channel_sum
    b = ImageBuf()
    ImageBufAlgo.channel_sum (b, ImageBuf("../oiiotool/tahoe-small.tif"),
                              (.2126,.7152,.0722))
    write (b, "chsum.tif", oiio.UINT8)

    # premult/unpremult
    b = constimage(100,100,4,oiio.FLOAT,(.1,.1,.1,1))
    ImageBufAlgo.fill (b, (.2,.2,.2,.5), oiio.ROI(50,80,50,80))
    ImageBufAlgo.unpremult (b, b)
    write (b, "unpremult.tif")
    ImageBufAlgo.premult (b, b)
    write (b, "premult.tif")

    b = ImageBuf ("../oiiotool/tahoe-small.tif")
    ImageBufAlgo.rangecompress (b, b)
    write (b, "rangecompress.tif", oiio.UINT8)
    ImageBufAlgo.rangeexpand (b, b)
    write (b, "rangeexpand.tif", oiio.UINT8)

    # FIXME - colorconvert, ociolook need tests

    # computePixelStats

    compresults = oiio.CompareResults()
    ImageBufAlgo.compare (ImageBuf("flip.tif"), ImageBuf("flop.tif"),
                          1.0e-6, 1.0e-6, compresults)
    print "Comparison: of flip.tif and flop.tif"
    print "  mean =", compresults.meanerror
    print "  rms  =", compresults.rms_error
    print "  PSNR =", compresults.PSNR
    print "  max  =", compresults.maxerror
    print "  max @", (compresults.maxx, compresults.maxy, compresults.maxz, compresults.maxc)
    print "  warns", compresults.nwarn, "fails", compresults.nfail

    # compare_Yee,
    # isConstantColor, isConstantChannel

    b = ImageBuf (ImageSpec(256,256,3,oiio.UINT8));
    ImageBufAlgo.fill (b, (1,0.5,0.5))
    r = ImageBufAlgo.isConstantColor (b)
    print "isConstantColor on pink image is", r
    r = ImageBufAlgo.isConstantColor (checker)
    print "isConstantColor on checker is ", r

    b = ImageBuf("cmul1.exr")
    print "Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b)
    b = ImageBuf("cmul2.exr")
    print "Is", b.name, "monochrome? ", ImageBufAlgo.isMonochrome(b)


    # color_count, color_range_check

    # nonzero_region
    b = constimage (256,256,3,oiio.UINT8,(0,0,0))
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

    # make_kernel
    bsplinekernel = ImageBuf()
    ImageBufAlgo.make_kernel (bsplinekernel, "bspline", 15, 15)
    write (bsplinekernel, "bsplinekernel.exr")

    # convolve
    b = ImageBuf()
    ImageBufAlgo.convolve (b, ImageBuf("../oiiotool/tahoe-small.tif"),
                           bsplinekernel)
    write (b, "bspline-blur.tif", oiio.UINT8)

    # unsharp_mask
    b = ImageBuf()
    ImageBufAlgo.unsharp_mask (b, ImageBuf("../oiiotool/tahoe-small.tif"),
                               "gaussian", 3.0, 1.0, 0.0)
    write (b, "unsharp.tif", oiio.UINT8)

    # computePixelHashSHA1
    print ("SHA-1 of bsplinekernel.exr is: " + 
           ImageBufAlgo.computePixelHashSHA1(bsplinekernel))

    # fft, ifft
    fft = ImageBuf()
    blue = ImageBuf()
    ImageBufAlgo.channels (blue, ImageBuf("../oiiotool/tahoe-small.tif"),
                           (2,))
    ImageBufAlgo.fft (fft, blue)
    write (fft, "fft.exr", oiio.HALF)
    inv = ImageBuf()
    ImageBufAlgo.ifft (inv, fft)
    b = ImageBuf()
    ImageBufAlgo.channels (b, inv, (0,0,0))
    write (b, "ifft.exr", oiio.HALF)
    inv.clear()
    fft.clear()

    # fixNonFinite
    bad = ImageBuf ("../oiiotool-fixnan/bad.exr")
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
    ImageBufAlgo.over (b, ImageBuf("../oiiotool-composite/a.exr"),
                       ImageBuf("../oiiotool-composite/b.exr"))
    write (b, "a_over_b.exr")

    # FIXME - no test for zover (not in oiio-composite either)

    # FIXME - no test for render_text (not in oiiotool, either)

    # histogram, histogram_draw,

    # make_texture
    ImageBufAlgo.make_texture (oiio.MakeTxTexture,
                               ImageBuf("../oiiotool/tahoe-small.tif"),
                               "tahoe-small.tx")

    # capture_image - no test

    print "Done."
except Exception as detail:
    print "Unknown exception:", detail

