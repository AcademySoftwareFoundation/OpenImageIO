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

The COD segment (0xFF52) follows, with its length at +2, Scod at +4, the
number of decomposition levels at +9, and the codeblock width and height
exponents (minus 2) at +10/+11. Setting bit 0 of Scod appends one precinct
size byte per resolution.

A DFS marker segment (0xFF72) holds an index and 2 bits per level giving the
kind of decomposition; a COC segment (0xFF53) for a component then refers to
it by setting bit 7 of its decomposition level byte, with the index in the
low bits.
"""

import struct
import sys

SIZ_XSIZ = 6
SIZ_XTSIZ = 22


def patch_size(base, width, height, tile_width=None, tile_height=None):
    d = bytearray(base)
    i = d.find(b'\xff\x51')
    if i < 0:
        raise RuntimeError('no SIZ marker found')
    struct.pack_into('>II', d, i + SIZ_XSIZ, width, height)
    struct.pack_into('>II', d, i + SIZ_XTSIZ, tile_width or width,
                     tile_height or height)
    return bytes(d)


def patch_cod(base, levels, xcb, ycb, precincts=None):
    d = bytearray(base)
    i = d.find(b'\xff\x52')
    if i < 0:
        raise RuntimeError('no COD marker found')
    (length,) = struct.unpack_from('>H', d, i + 2)
    d[i + 9] = levels
    d[i + 10] = xcb - 2
    d[i + 11] = ycb - 2
    if precincts is not None:
        d[i + 4] |= 1
        end = i + 2 + length
        d[end:end] = bytes(precincts)
        struct.pack_into('>H', d, i + 2, length + len(precincts))
    return bytes(d)


def add_dfs(base, levels, precinct):
    """Make every level of every component a NO_DWT one, which keeps the
    full component size at all resolutions."""
    d = bytearray(base)
    i = d.find(b'\xff\x52')
    if i < 0:
        raise RuntimeError('no COD marker found')
    (length,) = struct.unpack_from('>H', d, i + 2)
    cbstyle, xform = d[i + 12], d[i + 13]
    ncomps = struct.unpack_from('>H', d, d.find(b'\xff\x51') + 38)[0]
    nbytes = (levels + 3) // 4
    seg = struct.pack('>HHHB', 0xFF72, 5 + nbytes, 1, levels) + bytes(nbytes)
    for c in range(ncomps):
        seg += struct.pack('>HHBBBBBBB', 0xFF53, 9 + levels + 1, c, 1,
                           0x80 | 1, d[i + 10], d[i + 11], cbstyle, xform)
        seg += bytes([precinct] * (levels + 1))
    end = i + 2 + length
    d[end:end] = seg
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

    # Only 3 MB of pixels, but in 4096 16x16 tiles. OpenJPH allocates full
    # codeblock buffers for every subband of every tile, about 3 GB in all,
    # so the reader must estimate that and reject it.
    write('bomb-tiles.j2c', patch_size(base, 16, 65536, 16, 16))

    # Tiles so small that each of their subbands is under a pixel wide, yet
    # each still gets a full codeblock buffer: about 5 GB for 196 KB of pixels.
    write('bomb-tiny-tiles.j2c', patch_size(base, 16, 4095, 1, 1))

    # No decompositions and 4x256 codeblocks in 1x1 tiles. Each codeblock
    # buffer row is padded to 16 samples of up to 64 bits, so about 6 GB.
    write('bomb-narrow-blocks.j2c',
          patch_cod(patch_size(base, 16, 4095, 1, 1), 0, 2, 8))

    # One tile, but 1x1 precincts force 1x1 codeblocks, and OpenJPH allocates
    # a header for every codeblock and an object for every precinct: many GB
    # for 192 MB of pixels.
    write('bomb-tiny-precincts.j2c',
          patch_cod(patch_size(base, 128, 524288), 0, 2, 2, [0]))

    # One row, so a single row of 1x1 codeblocks, and OpenJPH allocates an
    # object, a buffer and a header for each: over 1 GB for 3 MB of pixels.
    write('bomb-wide-row.j2c',
          patch_cod(patch_size(base, 1048576, 1), 0, 2, 2, [0]))

    # One tile and the default precincts, so a valid file could be this
    # small, but 4x1024 codeblocks along a single row: OpenJPH allocates a
    # full 4x1024 buffer for each of 786432 codeblocks, over 50 GB.
    write('bomb-tall-blocks.j2c',
          patch_cod(patch_size(base, 1048576, 1), 0, 2, 10))

    # A DFS marker making every level a NO_DWT one, so all 17 resolutions
    # stay full size, each with 2x2 precincts: GBs for 6 MB of pixels.
    write('bomb-dfs.j2c',
          add_dfs(patch_cod(patch_size(base, 1024, 2048), 16, 6, 6,
                            [0x11] * 17), 16, 0x11))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
