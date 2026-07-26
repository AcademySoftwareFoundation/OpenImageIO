#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed-ICC .jpg fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make-icc-fixtures.py .

Each fixture is a valid 1x1 JPEG with a hand-built APP2 "ICC_PROFILE" payload
prepended, aimed at the shared ICC decoder (decode_icc_profile in
src/libOpenImageIO/icc.cpp) rather than at libjpeg:

  corrupt-icc-unaligned-mluc.jpg
        An 'mluc' (multi-localized unicode) tag placed at an odd byte offset
        inside the profile, whose English record points its UTF-16 string at
        another odd offset. The decoder read the record's 32-bit count fields
        and the 16-bit string code units directly through pointers at those
        file-controlled offsets, so both loads were misaligned (UBSan
        `alignment`, and a fault on strict-alignment CPUs). The reads are now
        done via memcpy.

  corrupt-icc-oversized.jpg
        The ICC header declares a profile_size far larger than the bytes that
        actually follow. Must be rejected cleanly (size mismatch) with a
        bounded read, never trusted as a length.
"""

import struct
import sys
import os

# A valid 1x1 grayscale JPEG written by oiiotool, with its APP1/APP2 segments
# stripped, embedded here so the fixtures don't depend on a JPEG writer and
# carry no version-dependent metadata of their own. (Same base image used by
# make-exif-fixtures.py.)
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


def app2_icc(profile):
    # APP2 marker carrying a single-chunk ICC_PROFILE payload:
    #   "ICC_PROFILE\0" + seq_no(1) + num_markers(1) + profile bytes
    body = b'ICC_PROFILE\0' + bytes([1, 1]) + profile
    return b'\xff\xe2' + struct.pack('>H', len(body) + 2) + body


def jpeg_with_icc(profile):
    # Insert the APP2 segment right after the SOI marker (first 2 bytes).
    return BASE_JPEG[:2] + app2_icc(profile) + BASE_JPEG[2:]


def icc_header(profile_size):
    # 128-byte ICC profile header. Everything is zero except the declared
    # profile_size (offset 0, big-endian) and the 'acsp' magic (offset 36).
    h = bytearray(128)
    struct.pack_into('>I', h, 0, profile_size & 0xFFFFFFFF)
    h[36:40] = b'acsp'
    return h


def icc_tag_table(tags):
    # tags: list of (signature4, offset, size). Returns tag_count + entries.
    out = bytearray()
    out += struct.pack('>I', len(tags))
    for sig, off, size in tags:
        assert len(sig) == 4
        out += sig + struct.pack('>II', off, size)
    return out


def make_unaligned_mluc():
    """'mluc' tag at an odd offset with an odd UTF-16 string offset."""
    header_len = 128
    table = icc_tag_table([(b'desc', 0, 0)])  # offset/size patched below
    # Lay the profile out so the mluc structure starts at an ODD offset. The
    # tag table ends at header_len + len(table); pad by whatever is needed to
    # reach an odd address.
    body_start = header_len + len(table)
    pad = 1 if (body_start % 2 == 0) else 0
    mluc_off = body_start + pad
    assert mluc_off % 2 == 1, "mluc must land on an odd offset"

    # mluc structure (offsets relative to mluc_off):
    #   0: "mluc"
    #   4: 0 (reserved)
    #   8: number of records (=1)
    #   12: record size (=12)
    #   16: record -> language 'en', country 0, len, string-offset
    #   <string-offset>: UTF-16BE string
    string_utf16 = 'Hi'.encode('utf-16-be')   # 4 bytes, 2 code units
    record_end = 16 + 12                       # first byte after the record
    str_rel = record_end                       # string offset relative to tag
    # mluc_off is odd and str_rel is even, so mluc_off+str_rel is odd -> the
    # UTF-16 read is misaligned too.
    assert (mluc_off + str_rel) % 2 == 1

    mluc = bytearray()
    mluc += b'mluc'
    mluc += b'\0\0\0\0'
    mluc += struct.pack('>I', 1)               # nrecords
    mluc += struct.pack('>I', 12)              # recordsize
    mluc += b'en'                              # language
    mluc += b'\0\0'                            # country
    mluc += struct.pack('>I', len(string_utf16))  # length in bytes
    mluc += struct.pack('>I', str_rel)         # string offset (rel to tag)
    mluc += string_utf16

    tag_size = len(mluc)
    total = mluc_off + tag_size

    prof = bytearray()
    prof += icc_header(total)
    prof += icc_tag_table([(b'desc', mluc_off, tag_size)])
    prof += b'\0' * pad
    prof += mluc
    assert len(prof) == total
    return bytes(prof)


def make_oversized():
    """Header declares a profile_size far larger than the actual bytes."""
    prof = bytearray()
    prof += icc_header(0x7FFFFFFF)   # claim ~2 GB
    prof += icc_tag_table([])        # tag_count = 0
    return bytes(prof)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else '.'
    fixtures = {
        'corrupt-icc-unaligned-mluc.jpg': make_unaligned_mluc(),
        'corrupt-icc-oversized.jpg': make_oversized(),
    }
    for name, profile in fixtures.items():
        path = os.path.join(outdir, name)
        with open(path, 'wb') as f:
            f.write(jpeg_with_icc(profile))
        print(f'wrote {path} ({os.path.getsize(path)} bytes)')


if __name__ == '__main__':
    main()
