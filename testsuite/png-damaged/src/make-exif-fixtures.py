#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed-Exif .png fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make-exif-fixtures.py .

These carry the same hand-built Exif payloads as the .jpg fixtures in
testsuite/jpeg-corrupt/src, but through PNG's eXIf chunk instead of JPEG's
APP1 marker. The two containers reach the same shared decoder, and the PNG
route matters on its own because an eXIf chunk's length field is 31 bits --
a JPEG APP1 marker caps out at 64 KB, so PNG is where a deeply nested IFD
chain can be made large enough to exhaust the stack.

  exif-utf8-type.png  A tag whose TIFF data type is 129, the Exif 3.0 UTF-8
                      type, which tiff_data_size() reported as size_t(-1).
  exif-deep-ifds.png  A chain of ExifIFD pointers, each aimed at the next.
"""

import binascii
import os
import struct
import sys
import zlib


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data
            + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff))


def png(exif_payload):
    # 1x1 8-bit grayscale, one zlib-compressed scanline (filter byte + pixel).
    ihdr = struct.pack('>IIBBBBB', 1, 1, 8, 0, 0, 0, 0)
    idat = zlib.compress(b'\0\0')
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'eXIf', exif_payload) + chunk(b'IDAT', idat)
            + chunk(b'IEND', b''))


def tiff_header(diroff=8):
    return b'II' + struct.pack('<HI', 42, diroff)


def entry(tag, typ, count, value):
    return struct.pack('<HHII', tag, typ, count, value)


def ifd(entries, next_ifd=0):
    out = struct.pack('<H', len(entries))
    for e in entries:
        out += e
    return out + struct.pack('<I', next_ifd)


def utf8_type_blob():
    return tiff_header() + ifd([entry(0x010e, 129, 1, 1)])


def deep_ifd_blob(depth=100):
    stride = 18
    blob = bytearray(tiff_header())
    for i in range(depth):
        at = 8 + stride * i
        blob += ifd([entry(0x8769, 4, 1, at + stride)])
    blob += struct.pack('<H', 0) + struct.pack('<I', 0)
    return bytes(blob)


def main(dir):
    with open(os.path.join(dir, 'exif-utf8-type.png'), 'wb') as f:
        f.write(png(utf8_type_blob()))
    with open(os.path.join(dir, 'exif-deep-ifds.png'), 'wb') as f:
        f.write(png(deep_ifd_blob()))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
