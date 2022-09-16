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

    print ("stat:cache_memory_used", ic.getattribute("stat:cache_memory_used", 'int64'))
    print ("stat:image_size", ic.getattribute("stat:image_size", 'int64'))
    total_files = ic.getattribute("total_files", 'int')
    print ("total_files", ic.getattribute("total_files", 'int'))
    print ("all_filenames", ic.getattribute("all_filenames", 'string[{}]'.format(total_files)))

    print ("\nDone.")
except Exception as detail:
    print ("Unknown exception:", detail)

