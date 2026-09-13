#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import OpenImageIO as oiio


# crash-subimage-seek-fail.tif holds two subimages: a 1x1 uint8 image whose
# single pixel is 17, followed by a 1048577x1 image that is a hair over the
# default limits:resolution, so it parses but seek_subimage() rejects it.
filename = "src/crash-subimage-seek-fail.tif"
sub0_value = 17


# A subimage that seek_subimage() rejects must not leave the reader claiming
# to still be on the subimage it was on before. If it does, seeking back
# takes the "we're already there" early out without repositioning the file,
# and the reads that follow use the rejected subimage's spec to fill a
# buffer sized for the old one.
def test_seek_back_after_rejection () :
    print ("Testing seek back after a rejected subimage:")
    input = oiio.ImageInput.open (filename)
    print ("  opened:", input is not None)
    # Probe subimage 1, then go back to 0 and read it -- the usual way to
    # find out how many subimages a file has.
    print ("  seek to subimage 1 rejected:", not input.seek_subimage (1, 0))
    input.geterror()
    print ("  seek back to subimage 0 ok:", input.seek_subimage (0, 0))
    spec = input.spec()
    print ("  spec is subimage 0's:", (spec.width, spec.height) == (1, 1))
    pixels = input.read_image (oiio.UINT8)
    print ("  read_image shape:", pixels.shape)
    print ("  read_image value:", int (pixels[0][0][0]),
           "(expected", str (sub0_value) + ")")
    input.close()
    print ("")


# ImageCache enumerates the subimages itself, hits the same rejection, and
# then reads subimage 0 through the ImageInput it kept.
def test_imagecache_after_rejection () :
    print ("Testing ImageCache read after a rejected subimage:")
    ic = oiio.ImageCache (shared=False)
    pixels = ic.get_pixels (filename, 0, 0, oiio.ROI (0, 1, 0, 1, 0, 1, 0, 1),
                            oiio.UINT8)
    print ("  get_pixels ok:", pixels is not None)
    print ("  get_pixels value:", int (pixels[0][0][0]),
           "(expected", str (sub0_value) + ")")
    print ("")


try:
    test_seek_back_after_rejection()
    test_imagecache_after_rejection()
    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
