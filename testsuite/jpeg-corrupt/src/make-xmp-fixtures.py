#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed-XMP .jpg fixture in this directory.

The file it writes is committed, so this only needs to be run if it must be
regenerated:

    python3 make-xmp-fixtures.py .

  corrupt-xmp-deep-nesting.jpg  A valid 1x1 JPEG whose APP1 XMP marker holds
                                an element chain nested one level per stack
                                frame in decode_xmp_node(). This is the JPEG
                                route to the shared decoder the .png fixtures
                                in testsuite/png-damaged/src reach through a
                                zTXt chunk. An APP1 marker caps out at 64 KB,
                                which buys ~9000 levels -- enough to exhaust
                                the stack of a debug or sanitizer build, or of
                                any worker thread with a small stack, and
                                enough to confirm the depth cap is not a
                                PNG-only property.
"""

import struct
import sys
import os

# The same valid 1x1 grayscale JPEG that make-exif-fixtures.py uses, written
# by oiiotool with its APP1 Exif segment stripped, embedded here so the
# fixture does not depend on a JPEG writer and carries no version-dependent
# metadata of its own.
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

XMP_URI = b'http://ns.adobe.com/xap/1.0/\0'

HEAD = ('<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>'
        '<x:xmpmeta xmlns:x="adobe:ns:meta/">'
        '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
        '<rdf:Description rdf:about="" '
        'xmlns:foo="http://example.com/foo/">')
FOOT = '</rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end="w"?>'


def app1_xmp(xmp):
    body = XMP_URI + xmp.encode('utf-8')
    assert len(body) + 2 <= 0xffff, 'APP1 payload exceeds the marker limit'
    return b'\xff\xe1' + struct.pack('>H', len(body) + 2) + body


def deep_nesting_xmp():
    # Fill the marker. The tags are left unclosed so a level costs 7 bytes
    # rather than 15; pugixml's fragment mode builds the nesting either way.
    budget = 0xffff - 2 - len(XMP_URI) - len(HEAD) - len(FOOT)
    return HEAD + '<foo:a>' * (budget // 7) + FOOT


def main(dir):
    path = os.path.join(dir, 'corrupt-xmp-deep-nesting.jpg')
    with open(path, 'wb') as f:
        f.write(BASE_JPEG[:2] + app1_xmp(deep_nesting_xmp()) + BASE_JPEG[2:])


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
