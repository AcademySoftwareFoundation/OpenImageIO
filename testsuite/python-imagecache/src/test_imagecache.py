#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import array
import numpy
import OpenImageIO as oiio



######################################################################
# main test starts here

try:
    ic = oiio.ImageCache()

    # Set some attributes
    ic.attribute("max_open_files", 90)
    ic.attribute("max_memory_MB", 900.0)
    ic.attribute("searchpath", "../common")
    print ("getattribute(\"max_open_files\")", ic.getattribute("max_open_files"))
    print ("getattribute(\"max_memory_MB\")", ic.getattribute("max_memory_MB"))
    print ("getattribute(\"searchpath\")", ic.getattribute("searchpath"))

    # Force a file to be touched by the IC and test get_imagespec
    spec = ic.get_imagespec("../common/tahoe-tiny.tif")
    print ("tahoe_tiny is", spec.width, "x", spec.height)
    spec = ic.get_imagespec("../common/grid.tif")
    print ("grid is", spec.width, "x", spec.height)

    # Test getattribute(name, type) with the full type specified
    print ("full getattribute stat:cache_memory_used", ic.getattribute("stat:cache_memory_used", 'int64'))
    print ("full getattribute stat:image_size", ic.getattribute("stat:image_size", 'int64'))
    total_files = ic.getattribute("total_files", 'int')
    print ("full getattribute total_files", ic.getattribute("total_files", 'int'))
    print ("full getattribute all_filenames", ic.getattribute("all_filenames", 'string[{}]'.format(total_files)))

    # Test getattributetype() to retrieve the type of an attribute
    print ("getattributetype stat:cache_memory_used", ic.getattributetype("stat:cache_memory_used"))
    print ("getattributetype stat:image_size", ic.getattributetype("stat:image_size"))
    print ("getattributetype total_files", ic.getattributetype("total_files"))
    print ("getattributetype all_filenames", ic.getattributetype("all_filenames"))

    # Test getattribute(name) with the type inferred from the attribute
    print ("untyped getattribute stat:cache_memory_used", ic.getattribute("stat:cache_memory_used"))
    print ("untyped getattribute stat:image_size", ic.getattribute("stat:image_size"))
    print ("untyped getattribute total_files", ic.getattribute("total_files"))
    print ("untyped getattribute all_filenames", ic.getattribute("all_filenames"))

    # Test getpixels()
    print ("getpixels from grid.tif:", ic.get_pixels("../common/grid.tif", 0, 0,
                                                     3, 5, 3, 5, 0, 1, "float"))
    print ("  has_error?", ic.has_error)
    print ("  geterror?", ic.geterror())
    print ("getpixels from broken.tif:", ic.get_pixels("broken.tif", 0, 0,
                                                     3, 5, 3, 5, 0, 1, "float"))
    print ("  has_error?", ic.has_error)
    print ("  geterror?", ic.geterror())

    print ("getstats beginning:")
    print (ic.getstats().split()[0:3])

    # invalidate some things
    ic.invalidate("../common/tahoe-tiny.tif")
    ic.invalidate_all(True)

    print ("\nDone.")
except Exception as detail:
    print ("Unknown exception:", detail)

