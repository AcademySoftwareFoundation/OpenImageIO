#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



############################################################################
# This file contains code examples from the ImageOutput chapter of the
# main OpenImageIO documentation.
#
# To add an additional test, replicate the section below. Change
# "example1" to a helpful short name that identifies the example.


# BEGIN-imageoutput-example1
import OpenImageIO as oiio
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

# END-imageoutput-example1

#
############################################################################



# BEGIN-imageoutput-simple
import OpenImageIO as oiio
import numpy as np

def simple_write() :
    filename = "simple.tif"
    xres = 320
    yres = 240
    channels = 3  # RGB
    pixels = np.zeros((yres, xres, channels), dtype=np.uint8)

    out = oiio.ImageOutput.create (filename)
    if out:
        spec = oiio.ImageSpec(xres, yres, channels, 'uint8')
        out.open (filename, spec)
        out.write_image (pixels)
        out.close ()
# END-imageoutput-simple


import OpenImageIO as oiio
import numpy as np

def scanlines_write() :
    filename = "scanlines.tif"
    xres = 320
    yres = 240
    channels = 3  # RGB
    spec = oiio.ImageSpec(xres, yres, channels, 'uint8')

    out = oiio.ImageOutput.create (filename)
    if out:
        # BEGIN-imageoutput-scanlines
        z = 0   # Always zero for 2D images
        out.open (filename, spec)
        for y in range(yres) :
            # Generate pixel array for one scanline.
            # As an example, we are just making a zero-filled scanline
            scanline = np.zeros((xres, channels), dtype=np.uint8)
            out.write_scanline (y, z, scanline)
        out.close ()
        # END-imageoutput-scanlines



if __name__ == '__main__':
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    simple_write()
    scanlines_write()
