#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the tiny crop-*.dng fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make_crop_dng.py .

The files contain only the bare minimum tags required to make them legal, 
validated with DNG SDK. All 4 files are identical apart from the orientation
tag.
"""

import os
import struct
import sys

BYTE, ASCII, SHORT, LONG = 1, 2, 3, 4

LINEAR_RAW = 34892


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


def make_dng(path, width=32, height=32,
    orientation = 1,
    image_insets = (0,0,0,0), # active area insets (t,l,b,r)
    crop_insets = (0,0,0,0), # crop insets relative to active area (t,l,b,r),
    ):
    """Write a single-IFD uncompressed Linear DNG."""
    model_str = b'OIIO Testcam\0'
    n_entries = 16
    ifd_off = 8
    heap_off = ifd_off + 2 + 12 * n_entries + 4

    # Lay out the data that doesn't fit in an entry's value field first, so
    # the tags can point at it.
    heap = bytearray()
    def place(data):
        off = heap_off + len(heap)
        heap.extend(data + b'\0' * (len(data) & 1))
        return off

    model_off = place(model_str)

    active_area = struct.pack('<HHHH',
        image_insets[0], image_insets[1],
        height - image_insets[2],
        width - image_insets[3])
    active_area_off = place(active_area)

    pixel_off = heap_off + len(heap)
    
    entries = [
        (0x00fe, LONG, 1, 0),                            # NewSubfileType
        (0x0100, LONG, 1, width),                        # ImageWidth
        (0x0101, LONG, 1, height),                       # ImageLength
        (0x0102, SHORT, 1, 16),                          # BitsPerSample
        (0x0103, SHORT, 1, 1),                           # Compression: none
        (0x0106, SHORT, 1, LINEAR_RAW),                  # PhotometricInterpretation
        (0x0111, LONG, 1, pixel_off),                    # StripOffsets
        (0x0112, SHORT, 1, orientation),                 # Orientation
        (0x0115, SHORT, 1, 1),                           # SamplesPerPixel
        (0x0116, LONG, 1, height),                       # RowsPerStrip
        (0x0117, LONG, 1, width * height * 2),           # StripByteCounts
        (0xc612, BYTE, 4, bytes([1, 4, 0, 0])),          # DNGVersion
        (0xc614, ASCII, len(model_str), model_off),      # UniqueCameraModel
        (0xc61f, SHORT, 2, struct.pack('<HH',            # DefaultCropOrigin
                crop_insets[1], crop_insets[0])),
        (0xc620, SHORT, 2, struct.pack('<HH',            # DefaultCropSize
                width - crop_insets[1] - crop_insets[3]
                      - image_insets[1] - image_insets[3],
                height - crop_insets[0] - crop_insets[2]
                      - image_insets[0] - image_insets[2])),
        (0xc68d, SHORT, 4, active_area_off),             # ActiveArea
    ]
    
    ifd = pack_ifd(entries)
    out = bytearray(b'II' + struct.pack('<HI', 42, ifd_off) + ifd + bytes(heap))
    assert len(out) == pixel_off
    
    # black outside of image margins,
    # gray outside of crop region
    # white otherwise
    for y in range(height):
        for x in range(width):
            if (y < image_insets[0] or
                y > height - image_insets[2] - 1 or
                x < image_insets[1] or
                x > width - image_insets[3] - 1):
                    out += bytes([0, 0])
            elif (y < image_insets[0] + crop_insets[0] or
                  y > height - image_insets[2] - crop_insets[2] - 1 or
                  x < image_insets[1] + crop_insets[1] or
                  x > width - image_insets[3] - crop_insets[3] - 1):
                    out += bytes([0, 128])
            else:
                out += bytes([255, 255])

    with open(path, 'wb') as f:
        f.write(bytes(out))


def make_crop_dng(dir, orientation, suffix):
    make_dng(os.path.join(dir, 'crop-36x32_' + suffix + '.dng'),
             width = 36, height = 32,
             orientation = orientation,
             image_insets=(6, 2, 4, 10),
             crop_insets=(2, 4, 8, 6))

def main(dir):
    make_crop_dng(dir, 1, '0')
    make_crop_dng(dir, 6, '90')
    make_crop_dng(dir, 3, '180')
    make_crop_dng(dir, 8, '270')

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
