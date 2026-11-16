#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Usage: test_eolnpad.py padded reference [padded reference ...]
#
# Compare each padded file against its unpadded reference both as a whole
# image and one scanline at a time. Reading rows individually is what
# exposes a reader that steps over end-of-line padding incorrectly, since a
# whole-image read starts every block at row 0.

from __future__ import annotations

import sys

import OpenImageIO as oiio


def check (filename, refname) :
    ref = oiio.ImageInput.open (refname)
    refpixels = ref.read_image (oiio.FLOAT)
    ref.close()
    input = oiio.ImageInput.open (filename)
    if input is None :
        print (filename, "failed to open:", oiio.geterror())
        return
    whole = input.read_image (oiio.FLOAT)
    whole_ok = whole is not None and (whole == refpixels).all()
    bad_rows = []
    for y in range (input.spec().height) :
        row = input.read_scanline (y, 0, oiio.FLOAT)
        if row is None or not (row == refpixels[y]).all() :
            bad_rows.append (y)
    input.close()
    print (filename, " whole image matches:", whole_ok,
           " mismatched scanlines:", bad_rows)


args = sys.argv[1:]
for i in range (0, len (args), 2) :
    check (args[i], args[i + 1])
