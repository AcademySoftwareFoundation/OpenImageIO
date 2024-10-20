#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



############################################################################
# This file contains code examples from the ImageInput chapter of the
# main OpenImageIO documentation.
#
# To add an additional test, replicate the section below. Change
# "example1" to a helpful short name that identifies the example.


# BEGIN-imageinput-example1
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

# END-imageinput-example1

#
############################################################################


# BEGIN-imageinput-simple
import OpenImageIO as oiio
def simple_read():
    filename = "tahoe.tif"

    inp = oiio.ImageInput.open(filename)
    if inp :
        spec = inp.spec()
        xres = spec.width
        yres = spec.height
        nchannels = spec.nchannels
        pixels = inp.read_image(0, 0, 0, nchannels, "uint8")
        inp.close()
# END-imageinput-simple


def scanlines_read() :
    filename = "scanlines.tif"

    # BEGIN-imageinput-scanlines
    inp = oiio.ImageInput.open (filename)
    spec = inp.spec()
    if spec.tile_width == 0 :
        for y in range(spec.height) :
            scanline = inp.read_scanline (y, 0, "uint8")
            # ... process data in scanline[0..width*channels-1] ...
    # else :
        # ... handle tiles, or reject the file ...
    inp.close ()
    # END-imageinput-scanlines

def tiles_read() :
    filename = "tiled.tif"

     # BEGIN-imageinput-tiles
    inp = oiio.ImageInput.open(filename)
    spec = inp.spec()
    if spec.tile_width == 0 :
        # ... read scanline by scanline ...
        pass
    else :
        # Tiles
        tilesize = spec.tile_width * spec.tile_height
        for y in range(0, spec.height, spec.tile_height) :
            for x in range(0, spec.width, spec.tile_width) :
                tile = inp.read_tile (x, y, 0, "uint8")
                # ... process the pixels in tile[][] ..
    inp.close ()
    # END-imageinput-tiles


def unassociated_alpha():
    filename = "unpremult.tif"

    # BEGIN-imageinput-unassociatedalpha
    # Set up an ImageSpec that holds the configuration hints.
    config = oiio.ImageSpec()
    config["oiio:UnassociatedAlpha"] = 1

    # Open the file, passing in the config.
    inp = oiio.ImageInput.open (filename, config)
    spec = inp.spec()
    pixels = inp.read_image (0, 0, 0, spec.nchannels, "uint8")
    if (spec["oiio:UnassociatedAlpha"] == 1):
        print("pixels holds unassociated alpha")
    else:
        print("pixels holds associated alpha")
    # END-imageinput-unassociatedalpha

def error_checking():
# BEGIN-imageinput-errorchecking
    import OpenImageIO as oiio
    import numpy as np

    filename = "tahoe.tif"
    inp = oiio.ImageInput.open(filename)
    if inp is None :
        print("Could not open", filename, ", error =", oiio.geterror())
        return
    spec = inp.spec()
    xres = spec.width
    yres = spec.height
    nchannels = spec.nchannels

    pixels = inp.read_image(0, 0, 0, nchannels, "uint8")
    if pixels is None :
        print("Could not read pixels from", filename, ", error =", inp.geterror())
        return

    if not inp.close() :
        print("Error closing", filename, ", error =", inp.geterror())
        return
# END-imageinput-errorchecking



if __name__ == '__main__':
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    simple_read()
    scanlines_read()
    tiles_read()
    unassociated_alpha()
    error_checking()
