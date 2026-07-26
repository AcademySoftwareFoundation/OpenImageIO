#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed-XMP .png fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make-xmp-fixtures.py .

Each fixture is a 1x1 PNG carrying a hostile XMP packet in a compressed zTXt
chunk keyed "XML:com.adobe.xmp", aimed at the shared XMP decoder rather than
at libpng. PNG is the useful container here because a zTXt chunk's length is
31 bits and its payload is deflated, so a packet large enough to matter costs
only a few KB on disk; a JPEG APP1 marker caps out at 64 KB.

  xmp-deep-nesting.png  Nested elements, one stack frame per level in
                        decode_xmp_node(). Unbounded before the depth cap.
  xmp-list-blowup.png   Many dc:subject list items. Each one re-split,
                        re-joined and re-interned the whole accumulated list,
                        so the cost was quadratic in the item count.
  xmp-many-attribs.png  Many distinct attributes. ImageSpec::attribute() is a
                        linear scan, so this was quadratic too.
"""

import binascii
import os
import struct
import sys
import zlib

XMP_KEY = b'XML:com.adobe.xmp'

HEAD = ('<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>'
        '<x:xmpmeta xmlns:x="adobe:ns:meta/">'
        '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
        '<rdf:Description rdf:about="" '
        'xmlns:dc="http://purl.org/dc/elements/1.1/" '
        'xmlns:foo="http://example.com/foo/">')
FOOT = '</rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end="w"?>'


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data
            + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff))


def png(xmp):
    # 1x1 8-bit grayscale, one zlib-compressed scanline (filter byte + pixel).
    ihdr = struct.pack('>IIBBBBB', 1, 1, 8, 0, 0, 0, 0)
    idat = zlib.compress(b'\0\0')
    ztxt = XMP_KEY + b'\0\0' + zlib.compress(xmp.encode('utf-8'), 9)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'zTXt', ztxt) + chunk(b'IDAT', idat)
            + chunk(b'IEND', b''))


def deep_nesting_xmp(depth=100000):
    return HEAD + '<foo:a>' * depth + '</foo:a>' * depth + FOOT


def list_blowup_xmp(items=20000):
    body = ''.join('<rdf:li dc:subject="k%d"/>' % i for i in range(items))
    return HEAD + body + FOOT


def many_attribs_xmp(count=8000):
    attrs = ''.join(' foo:a%d="v%d"' % (i, i) for i in range(count))
    return HEAD + '<rdf:li' + attrs + '/>' + FOOT


def main(dir):
    for name, xmp in [('xmp-deep-nesting.png', deep_nesting_xmp()),
                      ('xmp-list-blowup.png', list_blowup_xmp()),
                      ('xmp-many-attribs.png', many_attribs_xmp())]:
        with open(os.path.join(dir, name), 'wb') as f:
            f.write(png(xmp))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
