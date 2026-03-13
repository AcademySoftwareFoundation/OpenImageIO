#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

def run(oiio):
    r = oiio.ROI()
    print("undefined ROI() =", r)
    print("r.defined =", r.defined)
    print("r.nchannels =", r.nchannels)
    print("")

    r = oiio.ROI(0, 640, 100, 200)
    print("ROI(0, 640, 100, 200) =", r)
    r = oiio.ROI(0, 640, 0, 480, 0, 1, 0, 4)
    print("ROI(0, 640, 100, 480, 0, 1, 0, 4) =", r)
    print("r.xbegin =", r.xbegin)
    print("r.xend =", r.xend)
    print("r.ybegin =", r.ybegin)
    print("r.yend =", r.yend)
    print("r.zbegin =", r.zbegin)
    print("r.zend =", r.zend)
    print("r.chbegin =", r.chbegin)
    print("r.chend =", r.chend)
    print("r.defined = ", r.defined)
    print("r.width = ", r.width)
    print("r.height = ", r.height)
    print("r.depth = ", r.depth)
    print("r.nchannels = ", r.nchannels)
    print("r.npixels = ", r.npixels)
    print("")
    print("ROI.All =", oiio.ROI.All)
    print("")

    r2 = oiio.ROI(r)
    r3 = oiio.ROI(r)
    r3.xend = 320
    print("r == r2 (expect yes): ", (r == r2))
    print("r != r2 (expect no): ", (r != r2))
    print("r == r3 (expect no): ", (r == r3))
    print("r != r3 (expect yes): ", (r != r3))
    print("")

    print("r contains (10,10) (expect yes): ", r.contains(10, 10))
    print("r contains (1000,10) (expect no): ", r.contains(1000, 10))
    print("r contains roi(10,20,10,20,0,1,0,1) (expect yes): ",
          r.contains(oiio.ROI(10, 20, 10, 20, 0, 1, 0, 1)))
    print("r contains roi(1010,1020,10,20,0,1,0,1) (expect no): ",
          r.contains(oiio.ROI(1010, 1020, 10, 20, 0, 1, 0, 1)))

    a_roi = oiio.ROI(0, 10, 0, 8, 0, 1, 0, 4)
    b_roi = oiio.ROI(5, 15, -1, 10, 0, 1, 0, 4)
    print("A =", a_roi)
    print("B =", b_roi)
    print("ROI.union(A,B) =", oiio.union(a_roi, b_roi))
    print("ROI.intersection(A,B) =", oiio.intersection(a_roi, b_roi))
    print("")

    spec = oiio.ImageSpec()
    spec.x = 0
    spec.y = 0
    spec.z = 0
    spec.width = 640
    spec.height = 480
    spec.depth = 1
    spec.full_x = 0
    spec.full_y = 0
    spec.full_z = 0
    spec.full_width = 640
    spec.full_height = 480
    spec.full_depth = 1
    spec.nchannels = 3
    print("Spec's roi is", oiio.get_roi(spec))
    oiio.set_roi(spec, oiio.ROI(3, 5, 7, 9))
    oiio.set_roi_full(spec, oiio.ROI(13, 15, 17, 19))
    print("After set, roi is", oiio.get_roi(spec))
    print("After set, roi_full is", oiio.get_roi_full(spec))

    r1 = oiio.ROI(0, 640, 0, 480, 0, 1, 0, 4)
    r2 = r1.copy()
    r2.xbegin = 42
    print("r1 =", r1)
    print("r2 =", r2)
    print("")
    print("Done.")


def main() -> int:
    import OpenImageIO as oiio

    try:
        run(oiio)
    except Exception as detail:
        print("Unknown exception:", detail)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
