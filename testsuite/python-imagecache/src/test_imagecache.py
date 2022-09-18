#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import array
import numpy
import OpenImageIO as oiio



######################################################################
# main test starts here

try:
    ic = oiio.ImageCache()

    # Force a file to be touched by the IC
    ib = oiio.ImageBuf("../common/tahoe-tiny.tif")
    ib = oiio.ImageBuf("../common/grid.tif")

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

    print ("\nDone.")
except Exception as detail:
    print ("Unknown exception:", detail)

