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


# Section: Pattern Generation


# Section: Image transformation and data movement

def example_circular_shift() :
    # BEGIN-imagebufalgo-cshift
    A = ImageBuf("grid.exr")
    B = ImageBufAlgo.circular_shift(A, 70, 30)
    # END-imagebufalgo-cshift
    B.write("cshift.exr");


# Section: Image Arithmetic


# Section: Image comparison and statistics


# Section: Convolution and frequency-space algorithms


# Section: Image enhancement / restoration


# Section: Morphological filters


# Section: Color space conversion


# Section: Import / export






if __name__ == '__main__':
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    example1()

    # Section: ImageBufAlgo common principles

    # Section: Pattern Generation

    # Section: Image transformation and data movement
    example_circular_shift()

    # Section: Image Arithmetic

    # Section: Image comparison and statistics

    # Section: Convolution and frequency-space algorithms

    # Section: Image enhancement / restoration

    # Section: Morphological filters

    # Section: Color space conversion

    # Section: Import / export
