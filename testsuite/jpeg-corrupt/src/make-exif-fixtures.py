#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed-Exif .jpg fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make-exif-fixtures.py .

Each fixture is a valid 1x1 JPEG with a hand-built APP1 Exif payload prepended,
aimed at the shared Exif decoder rather than at libjpeg:

  corrupt-exif-utf8-type.jpg  A tag whose TIFF data type is 129, the Exif 3.0
                              UTF-8 type. tiff_data_size() knew nothing about
                              it and reported size_t(-1), which wrapped the
                              decoder's bounds arithmetic into a span of
                              length size_t(-1) (heap OOB read).
  corrupt-exif-deep-ifds.jpg  A chain of ExifIFD pointers, each aimed at the
                              next. Every level costs a stack frame; the
                              decoder now stops descending well before this.
"""

import struct
import sys
import os

# A valid 1x1 grayscale JPEG written by oiiotool, with its APP1 Exif
# segment stripped, embedded here so the fixtures don't depend on a JPEG
# writer and carry no version-dependent metadata of their own.
BASE_JPEG = bytes.fromhex(
    'ffd8ffe000104a46494600010100000100010000ffdb004300010101010101010101'
    '01010101010102010101010102010101020202020202020202030304030303030302'
    '020304030304040404040203050504040504040404ffc0000b080001000101011100'
    'ffc4001f0000010501010101010100000000000000000102030405060708090a0bff'
    'c400b5100002010303020403050504040000017d0102030004110512213141061351'
    '6107227114328191a1082342b1c11552d1f02433627282090a161718191a25262728'
    '292a3435363738393a434445464748494a535455565758595a636465666768696a73'
    '7475767778797a838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2'
    'b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8'
    'e9eaf1f2f3f4f5f6f7f8f9faffda0008010100003f00ff003ffaffd9')


def app1(exif_payload):
    body = b'Exif\0\0' + exif_payload
    return b'\xff\xe1' + struct.pack('>H', len(body) + 2) + body


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
    # ImageDescription declared as Exif 3.0 UTF-8 (type 129), one element,
    # with the value field holding an offset of 1.
    return tiff_header() + ifd([entry(0x010e, 129, 1, 1)])


def deep_ifd_blob(depth=100):
    # Each 18-byte IFD holds a single ExifIFD pointer aimed 18 bytes on.
    stride = 18
    blob = bytearray(tiff_header())
    for i in range(depth):
        at = 8 + stride * i
        blob += ifd([entry(0x8769, 4, 1, at + stride)])
    blob += struct.pack('<H', 0) + struct.pack('<I', 0)
    return bytes(blob)


def write(path, exif_payload):
    with open(path, 'wb') as f:
        f.write(BASE_JPEG[:2] + app1(exif_payload) + BASE_JPEG[2:])


def main(dir):
    write(os.path.join(dir, 'corrupt-exif-utf8-type.jpg'), utf8_type_blob())
    write(os.path.join(dir, 'corrupt-exif-deep-ifds.jpg'), deep_ifd_blob())


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
