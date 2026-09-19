#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the tiny .dng fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make_malformed_dng.py .

A DNG is just a TIFF, which makes it by far the cheapest way to hand LibRaw a
complete raw file that is small enough to commit. LibRaw refuses anything
under 22x22, so the valid control is 32x32 uncompressed CFA samples.

The malformed variants exercise the reader's open-time guards:

  bad-exif-type.dng   An Exif entry whose TIFF data type is out of range.
                      LibRaw hands the type and count to OIIO's exif parser
                      callback verbatim, and tiff_data_size() reports an
                      unknown type as size_t(-1).
  bomb-32000x32000.dng  Dimensions that imply ~2 GB of samples in a 280-byte
                      file, which only the decompression-ratio guard rejects
                      (the per-dimension and total-size limits pass).
  truncated.dng       Valid header, pixel data cut short.
"""

import os
import struct
import sys

BYTE, ASCII, SHORT, LONG = 1, 2, 3, 4

PHOTOMETRIC_CFA = 32803


def pack_ifd(entries):
    """Serialize one IFD, tags in ascending order. Each entry is
    (tag, type, count, value); value is an int or at most 4 bytes, both of
    which fit in the entry's value field."""
    body = bytearray()
    for tag, typ, count, value in sorted(entries):
        if isinstance(value, bytes):
            assert len(value) <= 4
            body += struct.pack('<HHI', tag, typ, count)
            body += value + b'\0' * (4 - len(value))
        else:
            body += struct.pack('<HHII', tag, typ, count, value)
    return struct.pack('<H', len(entries)) + bytes(body) + struct.pack('<I', 0)


def make_dng(path, width=32, height=32, exif=None, pixels=True, truncate=None):
    """Write a single-IFD uncompressed CFA DNG. `exif` is an optional
    (tag, tifftype, count) triple to put in an Exif sub-IFD."""
    make_str, model_str = b'OIIO\0', b'Testcam\0'
    n_entries = 15 + (exif is not None)
    ifd_off = 8
    heap_off = ifd_off + 2 + 12 * n_entries + 4

    # Lay out the data that doesn't fit in an entry's value field first, so
    # the tags can point at it.
    heap = bytearray()
    def place(data):
        off = heap_off + len(heap)
        heap.extend(data + b'\0' * (len(data) & 1))
        return off

    make_off = place(make_str)
    model_off = place(model_str)
    if exif is not None:
        exif_ifd = (struct.pack('<H', 1)
                    + struct.pack('<HHII', exif[0], exif[1], exif[2], 0)
                    + struct.pack('<I', 0))
        exif_off = place(exif_ifd)
    pixel_off = heap_off + len(heap)

    entries = [
        (0x00fe, LONG, 1, 0),                            # NewSubfileType
        (0x0100, LONG, 1, width),                        # ImageWidth
        (0x0101, LONG, 1, height),                       # ImageLength
        (0x0102, SHORT, 1, 16),                          # BitsPerSample
        (0x0103, SHORT, 1, 1),                           # Compression: none
        (0x0106, SHORT, 1, PHOTOMETRIC_CFA),             # Photometric
        (0x010f, ASCII, len(make_str), make_off),        # Make
        (0x0110, ASCII, len(model_str), model_off),      # Model
        (0x0111, LONG, 1, pixel_off),                    # StripOffsets
        (0x0115, SHORT, 1, 1),                           # SamplesPerPixel
        (0x0116, LONG, 1, height),                       # RowsPerStrip
        (0x0117, LONG, 1, width * height * 2),           # StripByteCounts
        (0x828d, SHORT, 2, struct.pack('<HH', 2, 2)),    # CFARepeatPatternDim
        (0x828e, BYTE, 4, bytes([0, 1, 1, 2])),          # CFAPattern: RGGB
        (0xc612, BYTE, 4, bytes([1, 4, 0, 0])),          # DNGVersion
    ]
    if exif is not None:
        entries.append((0x8769, LONG, 1, exif_off))

    ifd = pack_ifd(entries)
    out = bytearray(b'II' + struct.pack('<HI', 42, ifd_off) + ifd + bytes(heap))
    assert len(out) == pixel_off
    if pixels:
        for y in range(height):
            for x in range(width):
                out += struct.pack('<H', ((x * 4 + y * 7) % 256) << 6)
    if truncate is not None:
        del out[truncate:]
    with open(path, 'wb') as f:
        f.write(bytes(out))


def main(dir):
    make_dng(os.path.join(dir, 'valid-32x32.dng'))
    make_dng(os.path.join(dir, 'bad-exif-type.dng'),
             exif=(0x829a, 42, 1))  # 0x829a = ExposureTime, type 42 = bogus
    make_dng(os.path.join(dir, 'bomb-32000x32000.dng'),
             width=32000, height=32000, pixels=False)
    make_dng(os.path.join(dir, 'truncated.dng'), truncate=1024)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
