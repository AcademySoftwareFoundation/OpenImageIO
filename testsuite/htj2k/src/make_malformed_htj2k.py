#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed HTJ2K fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make_malformed_htj2k.py .

It patches the SIZ marker segment of the committed valid-16x16.j2c, so it
needs no J2K encoder. valid-16x16.j2c itself was produced with:

    oiiotool --pattern checker 16x16 3 -d uint8 -o valid-16x16.j2c

A raw J2K codestream starts with SOC (0xFF4F) followed by SIZ (0xFF51). Within
the SIZ segment, counting from the marker itself, Xsiz/Ysiz are at +6/+10 and
the tile size XTsiz/YTsiz at +22/+26. Those four fields are all the reader
consults for the image dimensions, so patching them is enough to make a tiny
file claim an enormous image.
"""

import struct
import sys

SIZ_XSIZ = 6
SIZ_XTSIZ = 22


def patch_size(base, width, height):
    d = bytearray(base)
    i = d.find(b'\xff\x51')
    if i < 0:
        raise RuntimeError('no SIZ marker found')
    struct.pack_into('>II', d, i + SIZ_XSIZ, width, height)
    struct.pack_into('>II', d, i + SIZ_XTSIZ, width, height)
    return bytes(d)


def main(outdir):
    with open(outdir + '/valid-16x16.j2c', 'rb') as f:
        base = f.read()

    def write(name, data):
        with open(outdir + '/' + name, 'wb') as f:
            f.write(data)
        print('wrote %s (%d bytes)' % (name, len(data)))

    # Past the per-dimension ceiling ("limits:resolution"). Before the guards
    # were added this reached m_buf.resize() and aborted the process with an
    # uncaught std::bad_alloc.
    write('bomb-resolution.j2c', patch_size(base, 2000000000, 2000000000))

    # Under both the per-dimension ceiling and "limits:imagesize_MB" (30 GB),
    # so only the declared-size-to-file-size ratio can reject this one.
    write('bomb-ratio.j2c', patch_size(base, 100000, 100000))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
