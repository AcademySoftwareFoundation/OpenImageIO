#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



############################################################################
# This file contains code examples from the ImageBufAlgo chapter of the
# main OpenImageIO documentation.
#
# To add an additional test, replicate the section below. Change
# "example1" to a helpful short name that identifies the example.


# BEGIN-imagebufalgo-example1
import OpenImageIO as oiio
from OpenImageIO import *
import numpy as np


def example1():
    print("example1")
    #
    # Example code fragment from the docs goes here.
    #
    # It probably should generate either some text output (which will show up
    # in "out.txt" that captures each test's output), or it should produce a
    # (small) image file that can be compared against a reference image that
    # goes in the ref/ subdirectory of this test.
    #
    return

# END-imagebufalgo-example1

#
############################################################################


# Section: ImageBufAlgo common principles

def example_output_error1():
    print("example_output_error1")
    fg = ImageBuf()
    bg = ImageBuf()

    # BEGIN-imagebufalgo-output-error1
    # Method 1: Return an image result
    dst = ImageBufAlgo.over(fg, bg)
    if dst.has_error:
        print("error:", dst.geterror())
    # END-imagebufalgo-output-error1


def example_output_error2():
    print("example_output_error2")
    fg = ImageBuf()
    bg = ImageBuf()

    # BEGIN-imagebufalgo-output-error2
    # Method 2: Write into an existing image
    dst = ImageBuf()  # will be the output image
    ok = ImageBufAlgo.over(dst, fg, bg)
    if not ok:
        print("error:", dst.geterror())
    # END-imagebufalgo-output-error2


# Section: Pattern Generation

def example_zero():
    print("example_zero")
    A = ImageBuf("grid.exr")
    B = ImageBuf("grid.exr")
    C = ImageBuf("grid.exr")

    # BEGIN-imagebufalgo-zero
    # Create a new 3-channel, 512x512 float image filled with 0.0 values.
    zero = ImageBufAlgo.zero(ROI(0, 512, 0, 512, 0, 1, 0, 3))

    # Zero out an existing buffer, keeping it the same size and data type
    ImageBufAlgo.zero(A)

    # Zero out just the green channel, leave everything else the same
    roi = B.roi
    roi.chbegin = 1 # green
    roi.chend = 2   # one past the end of the channel region
    ImageBufAlgo.zero(B, roi)

    # Zero out a rectangular region of an existing buffer
    ImageBufAlgo.zero(C, ROI(0, 100, 0, 100))
    # END-imagebufalgo-zero

    zero.write("zero1.exr", "half")
    A.write("zero2.exr", "half")
    B.write("zero3.exr", "half")
    C.write("zero4.exr", "half")


def example_fill():
    print("example_fill")
    # BEGIN-imagebufalgo-fill
    # Create a new 640x480 RGB image, with a top-to-bottom gradient
    # from red to pink
    pink = (1, 0.7, 0.7)
    red = (1, 0, 0)
    A = ImageBufAlgo.fill(top=red, bottom=pink,
                          roi=ROI(0, 640, 0, 480, 0, 1, 0, 3))

    # Draw a filled red rectangle overtop existing image A.
    ImageBufAlgo.fill(A, red, ROI(50, 100, 75, 175))
    # END-imagebufalgo-fill

    A.write("fill.exr", "half")


def example_checker():
    print("example_checker")
    # BEGIN-imagebufalgo-checker
    # Create a new 640x480 RGB image, fill it with a two-toned gray
    # checkerboard, the checkers being 64x64 pixels each.
    A = ImageBuf(ImageSpec(640, 480, 3, "float"))
    dark = (0.1, 0.1, 0.1)
    light = (0.4, 0.4, 0.4)
    A = ImageBufAlgo.checker(64, 64, 1, dark, light,
                             roi=ROI(0, 640, 0, 480, 0, 1, 0, 3))
    # END-imagebufalgo-checker

    A.write("checker.exr", "half")


def example_noise1():
    print("example_noise1")
    # BEGIN-imagebufalgo-noise1
    # Create a new 256x256 field of grayscale white noise on [0,1)
    A = ImageBufAlgo.noise("uniform", A=0.0, B=1.0, mono=True, seed=1,
                           roi=ROI(0, 256, 0, 256, 0, 1, 0, 3))

    # Create a new 256x256 field of grayscale blue noise on [0,1)
    B = ImageBufAlgo.noise("blue", A=0.0, B=1.0, mono=True, seed=1,
                           roi=ROI(0, 256, 0, 256, 0, 1, 0, 3))

    # Add color Gaussian noise to an existing image
    C = ImageBuf("tahoe.tif")
    ImageBufAlgo.noise(C, "gaussian", A=0.0, B=0.1, mono=False, seed=1)

    # Use salt and pepper noise to make occasional random dropouts
    D = ImageBuf("tahoe.tif")
    ImageBufAlgo.noise(D, "salt", A=0.0, B=0.01, mono=True, seed=1)
    # END-imagebufalgo-noise1

    A.write("noise1.exr", "half")
    B.write("noise2.exr", "half")
    C.write("noise3.exr", "half")
    D.write("noise4.exr", "half")


def example_noise2():
    print("example_noise2")
    # BEGIN-imagebufalgo-noise2
    A = ImageBufAlgo.bluenoise_image()
    # END-imagebufalgo-noise2

    A.write("blue-noise.exr", "half")


def example_point():
    print("example_point")
    # BEGIN-imagebufalgo-point
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    red = (1, 0, 0, 1)
    ImageBufAlgo.render_point(A, 50, 100, red)
    # END-imagebufalgo-point

    A.write("point.exr", "half")


def example_lines():
    print("example_lines")
    # BEGIN-imagebufalgo-lines
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    red = (1, 0, 0, 1)
    ImageBufAlgo.render_line(A, 10, 60, 250, 20, red)
    ImageBufAlgo.render_line(A, 250, 20, 100, 190, red, True)
    # END-imagebufalgo-lines

    A.write("lines.exr", "half")


def example_box():
    print("example_box")
    # BEGIN-imagebufalgo-box
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    cyan = (0, 1, 1, 1)
    yellow_transparent = (0.5, 0.5, 0, 0.5)
    ImageBufAlgo.render_box(A, 150, 100, 240, 180, cyan)
    ImageBufAlgo.render_box(A, 100, 50, 180, 140, yellow_transparent, fill=True)
    # END-imagebufalgo-box

    A.write("box.exr", "half")


def example_text1():
    print("example_text1")
    ImgA = ImageBufAlgo.zero(ROI(0, 640, 0, 480, 0, 1, 0, 3))
    ImgB = ImageBufAlgo.zero(ROI(0, 640, 0, 480, 0, 1, 0, 3))

    # BEGIN-imagebufalgo-text1
    ImageBufAlgo.render_text(ImgA, 50, 100, "Hello, world")
    red = (1, 0, 0, 1)
    ImageBufAlgo.render_text(ImgA, 100, 200, "Go Big Red!",
                             fontsize=60, fontname="", textcolor=red)

    white = (1, 1, 1, 1)
    ImageBufAlgo.render_text(ImgB, 320, 240, "Centered",
                             fontsize=60, fontname="", textcolor=white,
                             alignx="center", aligny="center")
    # END-imagebufalgo-text1

    ImgA.write("text1.exr", "half")
    ImgB.write("text2.exr", "half")


def example_text2():
    print("example_text2")
    # BEGIN-imagebufalgo-text2
    # Render text centered in the image, using text_size to find out
    # the size we will need and adjusting the coordinates.
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    Aroi = A.roi
    size = ImageBufAlgo.text_size("Centered", 48, "Courier New")
    if size.defined:
        x = Aroi.xbegin + Aroi.width//2 - (size.xbegin + size.width//2)
        y = Aroi.ybegin + Aroi.height//2 - (size.ybegin + size.height//2)
        ImageBufAlgo.render_text(A, x, y, "Centered", 48, "Courier New")
    # END-imagebufalgo-text2


# Section: Image transformation and data movement

def example_channels():
    print("example_channels")
    RGBA = ImageBuf("grid.exr")
    BRGA = ImageBuf()

    # BEGIN-imagebufalgo-channels
    # Copy the first 3 channels of an RGBA, drop the alpha
    RGB = ImageBufAlgo.channels(RGBA, (0, 1, 2))

    # Copy just the alpha channel, making a 1-channel image
    Alpha = ImageBufAlgo.channels(RGBA, ("A",))

    # Swap the R and B channels
    success = ImageBufAlgo.channels(BRGA, RGBA,
                                    channelorder=(2, 1, 0, 3),
                                    newchannelnames=("R", "G", "B", "A"))

    # Add an alpha channel with value 1.0 everywhere to an RGB image,
    # keep the other channels with their old ordering, values, and
    # names.
    RGBA = ImageBufAlgo.channels(RGB,
                                 channelorder=(0, 1, 2, 1.0),
                                 newchannelnames=("", "", "", "A"))
    # END-imagebufalgo-channels

    RGBA.write("channels-rgba.exr", "half")
    RGB.write("channels-rgb.exr", "half")
    Alpha.write("channels-alpha.exr", "half")
    BRGA.write("channels-brga.exr", "half")


def example_channel_append():
    print("example_channel_append")
    Z = ImageBuf(ImageSpec(640, 480, 1, "float"))

    # BEGIN-imagebufalgo-channel-append
    RGBA = ImageBuf("grid.exr")
    RGBAZ = ImageBufAlgo.channel_append(RGBA, Z)
    # END-imagebufalgo-channel-append

    RGBAZ.write("channel-append.exr", "half")


def example_copy():
    print("example_copy")
    # BEGIN-imagebufalgo-copy
    # Set B to be a copy of A, but converted to float
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.copy(A, convert="float")
    # END-imagebufalgo-copy

    B.write("copy.exr", "half")


def example_crop():
    print("example_crop")
    # BEGIN-imagebufalgo-crop
    # Set B to be a 200x100 region of A starting at (50,50), trimming
    # the exterior away but leaving that region in its original position.
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.crop(A, ROI(50, 250, 50, 150))
    # END-imagebufalgo-crop

    B.write("crop.exr", "half")


def example_cut():
    print("example_cut")
    # BEGIN-imagebufalgo-cut
    # Set B to be a 200x100 region of A starting at (50,50), but
    # moved to the upper left corner so its new origin is (0,0).
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.cut(A, ROI(50, 250, 50, 150))
    # END-imagebufalgo-cut

    B.write("cut.exr", "half")


def example_paste():
    print("example_paste")
    # BEGIN-imagebufalgo-paste
    # Paste Fg on top of Bg, offset by (100,100)
    Bg = ImageBuf("grid.exr")
    Fg = ImageBuf("tahoe.tif")
    ImageBufAlgo.paste(Bg, 100, 100, 0, 0, Fg)
    # END-imagebufalgo-paste

    Bg.write("paste.exr", "half")


def example_rotate_n():
    print("example_rotate_n")
    # BEGIN-imagebufalgo-rotate-n
    A = ImageBuf("grid.exr")
    R90 = ImageBufAlgo.rotate90(A)
    R180 = ImageBufAlgo.rotate180(A)
    R270 = ImageBufAlgo.rotate270(A)
    # END-imagebufalgo-rotate-n

    R90.write("rotate-90.exr", "half")
    R180.write("rotate-180.exr", "half")
    R270.write("rotate-270.exr", "half")


def example_flip_flop_transpose():
    print("example_flip_flop_transpose")
    # BEGIN-imagebufalgo-flip-flop-transpose
    A = ImageBuf("grid.exr")
    B1 = ImageBufAlgo.flip(A)
    B2 = ImageBufAlgo.flop(A)
    B3 = ImageBufAlgo.transpose(A)
    # END-imagebufalgo-flip-flop-transpose

    B1.write("flip.exr", "half")
    B2.write("flop.exr", "half")
    B3.write("transpose.exr", "half")


def example_reorient():
    print("example_reorient")
    tmp = ImageBuf("grid.exr")
    tmp.specmod().attribute("Orientation", 8)
    tmp.write("grid-vertical.exr", "half")

    # BEGIN-imagebufalgo-reorient
    A = ImageBuf("grid-vertical.exr")
    A = ImageBufAlgo.reorient(A)
    # END-imagebufalgo-reorient

    A.write("reorient.exr", "half")


def example_circular_shift():
    print("example_circular_shift")
    # BEGIN-imagebufalgo-cshift
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.circular_shift(A, 70, 30)
    # END-imagebufalgo-cshift
    B.write("cshift.exr", "half")


def example_rotate():
    print("example_rotate")
    # BEGIN-imagebufalgo-rotate-angle
    Src = ImageBuf("grid.exr")
    Dst = ImageBufAlgo.rotate(Src, 45.0)
    # END-imagebufalgo-rotate-angle
    Dst.write("rotate-45.tif", "uint8")


def example_resize():
    print("example_resize")
    # BEGIN-imagebufalgo-resize
    # Resize the image to 640x480, using the default filter
    Src = ImageBuf("grid.exr")
    roi = ROI(0, 320, 0, 240, 0, 1, 0, Src.nchannels)
    Dst = ImageBufAlgo.resize(Src, roi=roi)
    # END-imagebufalgo-resize
    Dst.write("resize.tif", "uint8")


def example_resample():
    print("example_resample")

    # BEGIN-imagebufalgo-resample
    # Resample quickly to 320x240, with default interpolation
    Src = ImageBuf("grid.exr")
    roi = ROI(0, 320, 0, 240, 0, 1, 0, Src.nchannels)
    Dst = ImageBufAlgo.resample(Src, roi=roi)
    # END-imagebufalgo-resample
    Dst.write("resample.exr", "half")


def example_fit():
    print("example_fit")
    # BEGIN-imagebufalgo-fit
    # Resize to fit into a max of 640x480, preserving the aspect ratio
    Src = ImageBuf("grid.exr")
    roi = ROI(0, 320, 0, 240, 0, 1, 0, Src.nchannels)
    Dst = ImageBufAlgo.fit(Src, "", roi=roi)
    # END-imagebufalgo-fit
    Dst.write("fit.tif", "uint8")


def example_warp():
    print("example_warp")
    # BEGIN-imagebufalgo-warp
    M = (0.7071068,  0.7071068, 0,
         -0.7071068, 0.7071068, 0,
         20,         -8.284271, 1)
    Src = ImageBuf("grid.exr")
    Dst = ImageBufAlgo.warp(Src, M, filtername="lanczos3")
    # END-imagebufalgo-warp
    Dst.write("warp.exr", "half")

def example_demosaic():
    print("example_demosaic")
    # BEGIN-imagebufalgo-demosaic
    Src = ImageBuf("bayer.png")
    WB_RGBG = (2.0, 1.0, 1.5, 1.0)
    Dst = ImageBufAlgo.demosaic(Src, layout="BGGR",
      white_balance = WB_RGBG)
    # END-imagebufalgo-demosaic
    Dst.write("demosaic.png")

# Section: Image Arithmetic
def example_add():
    print("example_add")
    # BEGIN-imagebufalgo-add
    # Add images A and B
    A = ImageBuf("A.exr")
    B = ImageBuf("B.exr")
    Sum = ImageBufAlgo.add (A, B)

    # Add 0.2 to channels 0-2, but not to channel 3
    Sum_cspan = ImageBufAlgo.add (A, (0.2, 0.2, 0.2, 0.0))
    # END-imagebufalgo-add
    Sum.write("add.exr", "half")
    Sum_cspan.write("add_cspan.exr", "half")

def example_sub():
    print("example_sub")
    # BEGIN-imagebufalgo-sub
    A = ImageBuf("A.exr")
    B = ImageBuf("B.exr")
    Diff = ImageBufAlgo.sub (A, B)
    # END-imagebufalgo-sub
    Diff.write("sub.exr", "half")

def example_absdiff():
    print("example_absdiff")
    # BEGIN-imagebufalgo-absdiff
    A = ImageBuf("A.exr")
    B = ImageBuf("B.exr")
    Diff = ImageBufAlgo.absdiff (A, B)
    # END-imagebufalgo-absdiff
    Diff.write("absdiff.exr", "half")

def example_abs():
    print("example_abs")
    # BEGIN-imagebufalgo-absolute
    A = ImageBuf("grid.exr")
    Abs = ImageBufAlgo.abs (A)
    # END-imagebufalgo-absolute
    Abs.write("abs.exr", "half")

def example_scale():
    print("example_scale")
    # BEGIN-imagebufalgo-scale
    # Pixel-by-pixel multiplication of all channels of one image A by the single channel of the other image
    A = ImageBuf("A.exr")
    B = ImageBuf("mono.exr")
    Product = ImageBufAlgo.scale (A, B)
    #END-imagebufalgo-scale
    Product.write("scale.exr", "half")

def example_mul():
    print("example_mul")
    # BEGIN-imagebufalgo-mul
    # Pixel-by-pixel, channel-by-channel multiplication of A and B
    A = ImageBuf("A.exr")
    B = ImageBuf("B.exr")
    Product = ImageBufAlgo.mul (A, B)

    # In-place reduce intensity of A's channels 0-2 by 50%
    ImageBufAlgo.mul (A, A, (0.5, 0.5, 0.5, 1.0))
    # END-imagebufalgo-mul
    Product.write("mul.exr", "half")

def example_div():
    print("example_div")
    # BEGIN-imagebufalgo-div
    # Pixel-by-pixel, channel-by-channel division of A by B
    A = ImageBuf("A.exr")
    B = ImageBuf("B.exr")
    Ratio = ImageBufAlgo.div (A, B)

    # In-place reduce intensity of A's channels 0-2 by 50%
    ImageBufAlgo.div (A, A, (2.0, 2.0, 2.0, 1.0))
    # END-imagebufalgo-div
    Ratio.write("div.exr", "half")

#TODO: mad and onwards

# Section: Image comparison and statistics


# Section: Convolution and frequency-space algorithms


# Section: Image enhancement / restoration

def example_fixNonFinite():
    print("example_fixNonFinite")
    # BEGIN-imagebufalgo-fixNonFinite
    Src = ImageBuf("with_nans.tif")
    ImageBufAlgo.fixNonFinite (Src, Src, oiio.NONFINITE_BOX3)
    # END-imagebufalgo-fixNonFinite

    # fixing the nans seems nondeterministic - so not writing out the image
    # Src.write("with_nans_fixed.tif")

def example_fillholes_pushpull():
    print("example_fillholes_pushpull")
    # BEGIN-imagebufalgo-fillholes_pushpull
    Src = ImageBuf("checker_with_alpha.exr")
    Filled = ImageBufAlgo.fillholes_pushpull(Src)
    # END-imagebufalgo-fillholes_pushpull
    Filled.write("checker_with_alpha_filled.exr")


def example_median_filter():
    print("example_median_filter")
    # BEGIN-imagebufalgo-median_filter
    Noisy = ImageBuf("tahoe.tif")
    Clean = ImageBufAlgo.median_filter (Noisy, 3, 3)
    # END-imagebufalgo-median_filter
    Clean.write("tahoe_median_filter.tif")


def example_unsharp_mask():
    print("example_unsharp_mask")
    # BEGIN-imagebufalgo-unsharp_mask
    Blurry = ImageBuf("tahoe.tif")
    Sharp = ImageBufAlgo.unsharp_mask (Blurry, "gaussian", 5.0)
    # END-imagebufalgo-unsharp_mask
    Sharp.write("tahoe_unsharp_mask.tif")

# Section: Morphological filters


# Section: Color space conversion


# Section: Import / export

def example_make_texture():
    print("example_make_texture")
    # BEGIN-imagebufalgo-make-texture
    Input = ImageBuf("grid.exr")
    config = ImageSpec()
    config["maketx:highlightcomp"] = 1
    config["maketx:filtername"] = "lanczos3"
    config["maketx:opaque_detect"] = 1

    ok = ImageBufAlgo.make_texture (oiio.MakeTxTexture,
                                    Input, "texture.exr", config)
    if not ok :
        print("make_texture error:", oiio.geterror())

    # END-imagebufalgo-make-texture


if __name__ == '__main__':
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    example1()

    # Section: ImageBufAlgo common principles
    example_output_error1()
    example_output_error2()

    # Section: Pattern Generation
    example_zero()
    example_fill()
    example_checker()
    example_noise1()
    example_noise2()
    example_point()
    example_lines()
    example_box()
    example_text1()
    example_text2()

    # Section: Image transformation and data movement
    example_channels()
    example_channel_append()
    example_copy()
    example_crop()
    example_cut()
    example_paste()
    example_rotate_n()
    example_flip_flop_transpose()
    example_reorient()
    example_circular_shift()
    example_rotate()
    example_resize()
    example_resample()
    example_fit()
    example_warp()
    example_demosaic()

    # Section: Image Arithmetic
    example_add()
    example_sub()
    example_absdiff()
    example_abs()
    example_scale()
    example_mul()
    example_div()

    # Section: Image comparison and statistics

    # Section: Convolution and frequency-space algorithms

    # Section: Image enhancement / restoration
    example_fixNonFinite()
    example_fillholes_pushpull()
    example_median_filter()
    example_unsharp_mask()

    # Section: Morphological filters

    # Section: Color space conversion

    # Section: Import / export
    example_make_texture()
