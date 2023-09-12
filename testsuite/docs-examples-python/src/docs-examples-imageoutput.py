#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio

from __future__ import print_function
from __future__ import absolute_import


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
    simple_write()
    scanlines_write()
