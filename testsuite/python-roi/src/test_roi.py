#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import OpenImageIO as oiio





######################################################################
# main test starts here

try:
    r = oiio.ROI()
    print ("undefined ROI() =", r)
    print ("r.defined =", r.defined)
    print ("r.nchannels =", r.nchannels)
    print ("")

    r = oiio.ROI (0, 640, 100, 200)
    print ("ROI(0, 640, 100, 200) =", r)
    r = oiio.ROI (0, 640, 0, 480, 0, 1, 0, 4)
    print ("ROI(0, 640, 100, 480, 0, 1, 0, 4) =", r)
    print ("r.xbegin =", r.xbegin)
    print ("r.xend =", r.xend)
    print ("r.ybegin =", r.ybegin)
    print ("r.yend =", r.yend)
    print ("r.zbegin =", r.zbegin)
    print ("r.zend =", r.zend)
    print ("r.chbegin =", r.chbegin)
    print ("r.chend =", r.chend)
    print ("r.defined = ", r.defined)
    print ("r.width = ", r.width)
    print ("r.height = ", r.height)
    print ("r.depth = ", r.depth)
    print ("r.nchannels = ", r.nchannels)
    print ("r.npixels = ", r.npixels)
    print ("")
    print ("ROI.All =", oiio.ROI.All)
    print ("")

    r2 = oiio.ROI(r)
    r3 = oiio.ROI(r)
    r3.xend = 320
    print ("r == r2 (expect yes): ", (r == r2))
    print ("r != r2 (expect no): ", (r != r2))
    print ("r == r3 (expect no): ", (r == r3))
    print ("r != r3 (expect yes): ", (r != r3))
    print ("")

    print ("r contains (10,10) (expect yes): ", r.contains(10,10))
    print ("r contains (1000,10) (expect no): ", r.contains(1000,10))
    print ("r contains roi(10,20,10,20,0,1,0,1) (expect yes): ", r.contains(oiio.ROI(10,20,10,20,0,1,0,1)))
    print ("r contains roi(1010,1020,10,20,0,1,0,1) (expect no): ", r.contains(oiio.ROI(1010,1020,10,20,0,1,0,1)))

    A = oiio.ROI (0, 10, 0, 8, 0, 1, 0, 4)
    B = oiio.ROI (5, 15, -1, 10, 0, 1, 0, 4)
    print ("A =", A)
    print ("B =", B)
    print ("ROI.union(A,B) =", oiio.union(A,B))
    print ("ROI.intersection(A,B) =", oiio.intersection(A,B))
    print ("")

    spec = oiio.ImageSpec(640, 480, 3, oiio.UINT8)
    print ("Spec's roi is", oiio.get_roi(spec))
    oiio.set_roi (spec, oiio.ROI(3, 5, 7, 9))
    oiio.set_roi_full (spec, oiio.ROI(13, 15, 17, 19))
    print ("After set, roi is", oiio.get_roi(spec))
    print ("After set, roi_full is", oiio.get_roi_full(spec))

    r1 = oiio.ROI(0, 640, 0, 480, 0, 1, 0, 4)
    r2 = r1.copy()
    r2.xbegin = 42
    print ("r1 =", r1)
    print ("r2 =", r2)

    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

