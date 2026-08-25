#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



############################################################################
# This file contains code examples from the ImageCache chapter of the
# main OpenImageIO documentation.
#
# To add an additional test, replicate the section below. Change
# "example1" to a helpful short name that identifies the example.


# BEGIN-imagecache-example1
import OpenImageIO as oiio
import numpy as np

def example1() -> None:
    # Create an image cache and set some options
    cache = oiio.ImageCache()
    cache.attribute("max_memory_MB", 500.0)
    cache.attribute("autotile", 64)

    # Get information about a file
    spec = cache.get_imagespec("tahoe.tif")
    print(f"resolution is {spec.width}x{spec.height}")

    # Get a block of pixels from a file (returns a numpy array, or None
    # on failure).
    roi = oiio.ROI(0, 4, 0, 4, 0, 1, 0, spec.nchannels)
    pixels = cache.get_pixels("tahoe.tif", 0, 0, roi)

    # Note that all files were referenced by name, we never had to open
    # or close any files, and all the resource and memory management
    # was automatic.
    oiio.ImageCache.destroy(cache)
    return

# END-imagecache-example1

#
############################################################################





if __name__ == '__main__':
    print("docs-examples-imagecache.py")
    # Each example function needs to get called here, or it won't execute
    # as part of the test.
    example1()
