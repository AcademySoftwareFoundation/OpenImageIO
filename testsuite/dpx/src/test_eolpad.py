#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Make a copy of each named unpadded DPX with end-of-line padding inserted
# after every row. Usage: test_eolpad.py name.dpx ...  (writes name-eolpad.dpx)

import struct
import sys


def add_padding(src, dst, pad):
    with open(src, "rb") as f:
        d = bytearray(f.read())
    e = ">" if d[0:4] == b"SDPX" else "<"
    height = struct.unpack_from(e + "I", d, 776)[0]
    offset = struct.unpack_from(e + "I", d, 808)[0]
    rowbytes = (len(d) - offset) // height
    rows = [d[offset + y * rowbytes : offset + (y + 1) * rowbytes]
            for y in range(height)]
    out = d[:offset] + b"".join(r + b"\xee" * pad for r in rows)
    struct.pack_into(e + "I", out, 16, len(out))     # file size
    struct.pack_into(e + "I", out, 812, pad)         # end-of-line padding
    with open(dst, "wb") as f:
        f.write(out)


for name in sys.argv[1:]:
    add_padding(name, name.replace(".dpx", "-eolpad.dpx"), 8)
