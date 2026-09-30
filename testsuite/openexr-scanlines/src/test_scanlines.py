#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Read each test file scanline by scanline, and in swaths of assorted sizes
# and alignments, comparing everything against a single read of the whole
# image.

import OpenImageIO as oiio

def scanline_reads_match(filename):
    inp = oiio.ImageInput.open(filename)
    if inp is None:
        print("Could not open", filename, ":", oiio.geterror())
        return False
    spec = inp.spec()
    y0, height, nchans = spec.y, spec.height, spec.nchannels
    full = inp.read_image(oiio.HALF)
    if full is None:
        print("Could not read", filename, ":", inp.geterror())
        return False
    full = full.reshape(height, spec.width, nchans)
    ok = True
    for step in [1, 3, 7, 16, 17, 32, 64, 256]:
        for start in [0, 1, 15, 31]:
            y = y0 + start
            while y < y0 + height:
                yend = min(y + step, y0 + height)
                pixels = inp.read_scanlines(0, 0, y, yend, 0, 0, nchans, oiio.HALF)
                if pixels is None:
                    print("read_scanlines({},{}) of {} failed: {}".format(
                          y, yend, filename, inp.geterror()))
                    return False
                pixels = pixels.reshape(yend - y, spec.width, nchans)
                if not (pixels == full[y - y0 : yend - y0]).all():
                    print("{}: wrong pixels for scanlines {}-{} (step {})".format(
                          filename, y, yend, step))
                    ok = False
                y = yend
    # One scanline at a time, of channel subsets
    for (chbegin, chend) in [(0, 1), (1, 3), (3, 4)]:
        for y in range(y0, y0 + height):
            pixels = inp.read_scanlines(0, 0, y, y + 1, 0, chbegin, chend, oiio.HALF)
            pixels = pixels.reshape(spec.width, chend - chbegin)
            if not (pixels == full[y - y0, :, chbegin:chend]).all():
                print("{}: wrong pixels for scanline {}, channels {}-{}".format(
                      filename, y, chbegin, chend))
                ok = False
                break
    inp.close()
    return ok


for filename in ["none.exr", "zips.exr", "zip.exr", "piz.exr", "dwab.exr",
                 "offset.exr"]:
    print(filename, "scanline reads match:",
          "PASS" if scanline_reads_match(filename) else "FAIL")
