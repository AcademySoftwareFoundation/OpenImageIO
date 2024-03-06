#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import print_function
from __future__ import absolute_import


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

def example1() :
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
    fg = ImageBuf()
    bg = ImageBuf()

    # BEGIN-imagebufalgo-output-error1
    # Method 1: Return an image result
    dst = ImageBufAlgo.over(fg, bg)
    if dst.has_error:
        print("error:", dst.geterror())
    # END-imagebufalgo-output-error1


def example_output_error2():
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

    zero.write("zero1.exr")
    A.write("zero2.exr")
    B.write("zero3.exr")
    C.write("zero4.exr")


def example_fill():
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

    A.write("fill.exr")


def example_checker():
    # BEGIN-imagebufalgo-checker
    # Create a new 640x480 RGB image, fill it with a two-toned gray
    # checkerboard, the checkers being 64x64 pixels each.
    A = ImageBuf(ImageSpec(640, 480, 3, "float"))
    dark = (0.1, 0.1, 0.1)
    light = (0.4, 0.4, 0.4)
    A = ImageBufAlgo.checker(64, 64, 1, dark, light,
                             roi=ROI(0, 640, 0, 480, 0, 1, 0, 3))
    # END-imagebufalgo-checker

    A.write("checker.exr")


def example_noise1():
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

    A.write("noise1.exr")
    B.write("noise2.exr")
    C.write("noise3.exr")
    D.write("noise4.exr")


def example_noise2():
    # BEGIN-imagebufalgo-noise2
    A = ImageBufAlgo.bluenoise_image()
    # END-imagebufalgo-noise2

    A.write("blue-noise.exr")


def example_point():
    # BEGIN-imagebufalgo-point
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    red = (1, 0, 0, 1)
    ImageBufAlgo.render_point(A, 50, 100, red)
    # END-imagebufalgo-point

    A.write("point.exr")


def example_lines():
    # BEGIN-imagebufalgo-lines
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    red = (1, 0, 0, 1)
    ImageBufAlgo.render_line(A, 10, 60, 250, 20, red)
    ImageBufAlgo.render_line(A, 250, 20, 100, 190, red, True)
    # END-imagebufalgo-lines

    A.write("lines.exr")


def example_box():
    # BEGIN-imagebufalgo-box
    A = ImageBuf(ImageSpec(640, 480, 4, "float"))
    cyan = (0, 1, 1, 1)
    yellow_transparent = (0.5, 0.5, 0, 0.5)
    ImageBufAlgo.render_box(A, 150, 100, 240, 180, cyan)
    ImageBufAlgo.render_box(A, 100, 50, 180, 140, yellow_transparent, fill=True)
    # END-imagebufalgo-box

    A.write("box.exr")


def example_text1():
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

    ImgA.write("text1.exr")
    ImgB.write("text2.exr")


def example_test2():
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

def example_circular_shift() :
    # BEGIN-imagebufalgo-cshift
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.circular_shift(A, 70, 30)
    # END-imagebufalgo-cshift
    B.write("cshift.exr")


# Section: Image Arithmetic


# Section: Image comparison and statistics


# Section: Convolution and frequency-space algorithms


# Section: Image enhancement / restoration


# Section: Morphological filters


# Section: Color space conversion


# Section: Import / export

def example_make_texture():
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
    example_test2()

    # Section: Image transformation and data movement
    example_circular_shift()

    # Section: Image Arithmetic

    # Section: Image comparison and statistics

    # Section: Convolution and frequency-space algorithms

    # Section: Image enhancement / restoration

    # Section: Morphological filters

    # Section: Color space conversion

    # Section: Import / export
    example_make_texture()
