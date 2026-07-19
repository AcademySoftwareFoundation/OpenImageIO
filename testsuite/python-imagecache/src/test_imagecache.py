#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import os

import numpy

import OpenImageIO as oiio


grid = "../common/grid.tif"
tahoe_tiny = "../common/tahoe-tiny.tif"





######################################################################
# main test starts here

try:
    ic = oiio.ImageCache()

    print ("Testing attribute() one-arg:")
    ic.attribute ("max_open_files", 90)
    ic.attribute ("max_memory_MB", 900.0)
    ic.attribute ("searchpath", "../common")
    print ("  max_open_files:", ic.getattribute ("max_open_files"))
    print ("  max_memory_MB:", ic.getattribute ("max_memory_MB"))
    print ("  searchpath:", ic.getattribute ("searchpath"))
    print ("")

    print ("Testing attribute() typed:")
    ic.attribute ("max_open_files", oiio.TypeInt, 42)
    print ("  max_open_files typed round-trip:",
           ic.getattribute ("max_open_files", oiio.TypeInt) == 42)
    print ("")

    print ("Testing get_imagespec:")
    spec = ic.get_imagespec (tahoe_tiny)
    print ("  tahoe_tiny is", spec.width, "x", spec.height)
    spec = ic.get_imagespec (grid)
    print ("  grid is", spec.width, "x", spec.height)
    print ("")

    print ("Testing resolve_filename:")
    resolved = ic.resolve_filename (grid)
    print ("  resolve ends with grid.tif:", resolved.endswith ("grid.tif"))
    print ("")

    print ("Testing get_cache_dimensions:")
    cache_spec = ic.get_cache_dimensions (grid, 0, 0)
    print ("  cache dimensions width>0:", cache_spec.width > 0)
    print ("  cache dimensions height>0:", cache_spec.height > 0)
    print ("")

    print ("Testing getattribute() and getattributetype():")
    print ("  getattributetype stat:cache_memory_used:",
           ic.getattributetype ("stat:cache_memory_used"))
    print ("  getattributetype stat:image_size:",
           ic.getattributetype ("stat:image_size"))
    print ("  getattributetype total_files:",
           ic.getattributetype ("total_files"))
    print ("  getattributetype searchpath:",
           ic.getattributetype ("searchpath"))
    total_files = ic.getattribute ("total_files", oiio.TypeInt)
    print ("  total_files is int:", isinstance (total_files, int))
    print ("  total_files >= 2:", total_files >= 2)
    image_size = ic.getattribute ("stat:image_size", oiio.TypeInt64)
    print ("  stat:image_size is int:", isinstance (image_size, int))
    print ("  stat:image_size positive:", image_size > 0)
    all_names = ic.getattribute ("all_filenames",
                                 oiio.TypeDesc ("string[{}]".format (total_files)))
    print ("  all_filenames is tuple:", isinstance (all_names, tuple))
    print ("  all_filenames len matches total_files:",
           len (all_names) == total_files)
    print ("  untyped stat:cache_memory_used is int:",
           isinstance (ic.getattribute ("stat:cache_memory_used"), int))
    print ("")

    print ("Testing get_pixels() coord overload:")
    pixels = ic.get_pixels (grid, 0, 0, 3, 5, 3, 5, 0, 1, "float")
    print ("  getpixels from grid.tif:", pixels)
    print ("  has_error after success:", ic.has_error)
    print ("")

    print ("Testing get_pixels() ROI overload:")
    roi = oiio.ROI (3, 5, 3, 5, 0, 1, 0, 4)
    pixels_roi = ic.get_pixels (grid, 0, 0, roi, "float")
    print ("  ROI matches coord overload:",
           numpy.array_equal (pixels_roi, pixels))
    print ("")

    print ("Testing get_pixels() missing file:")
    broken = ic.get_pixels ("broken.tif", 0, 0, 3, 5, 3, 5, 0, 1, "float")
    print ("  broken returns None:", broken is None)
    print ("  has_error after broken:", ic.has_error)
    print ("  geterror nonempty:", len (ic.geterror()) > 0)
    print ("")

    print ("Testing getstats:")
    stats = ic.getstats()
    print ("  getstats starts with OpenImageIO:",
           stats.startswith ("OpenImageIO"))
    print ("  getstats contains statistics:", "statistics" in stats)
    print ("")

    print ("Testing invalidate/invalidate_all:")
    ic.invalidate (tahoe_tiny, True)
    ic.invalidate_all (False)
    print ("  invalidate ok: True")
    print ("")

    print ("Testing ImageCache.destroy:")
    private_ic = oiio.ImageCache (shared=False)
    oiio.ImageCache.destroy (private_ic)
    print ("  destroy shared=False ok: True")
    print ("")

    print ("Testing has_error/geterror on private cache:")
    err_ic = oiio.ImageCache (shared=False)
    print ("  has_error before:", err_ic.has_error)
    err_ic.get_pixels ("no_such_imagecache_test.tif", 0, 0,
                       0, 1, 0, 1, 0, 1, "float")
    print ("  has_error after bad file:", err_ic.has_error)
    err1 = err_ic.geterror (clear=False)
    err2 = err_ic.geterror (clear=False)
    print ("  geterror persists:", err1 == err2 and len (err1) > 0)
    err3 = err_ic.geterror (clear=True)
    print ("  geterror clear returns message:", err3 == err1)
    print ("  has_error after clear:", err_ic.has_error)
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
