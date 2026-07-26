#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

"""Generator for the malformed .rla fixtures in this directory.

The files it writes are committed, so this only needs to be run if they must
be regenerated:

    python3 make_malformed_rla.py .

RLA is big-endian throughout: a 740-byte header, then one uint32 per scanline
giving that scanline's absolute file offset, then the RLE records. Because the
offset table costs only 4 bytes per scanline, a tiny file can describe an
enormous image -- which is what bomb.rla exercises.
"""

import struct
import sys

CT_BYTE, CT_WORD, CT_DWORD, CT_FLOAT = 0, 1, 2, 4

# Byte offsets of the fields we patch, within the 740-byte header.
OFF_WINDOW_LEFT = 0
OFF_ACTIVE_LEFT = 8
OFF_NEXT_OFFSET = 736


def header(active_w, active_h, nchan=1, nmatte=0, naux=0,
           chan_type=CT_BYTE, chan_bits=8,
           matte_type=CT_BYTE, matte_bits=8,
           aux_type=CT_BYTE, aux_bits=8,
           next_offset=0, field_rendered=0, revision=0xFFFE):
    def s(n):
        return struct.pack('>h', n)

    def l(n):
        return struct.pack('>i', n)

    def c(n, txt=b''):
        return txt.ljust(n, b'\0')

    h = b''
    h += s(0) + s(active_w - 1)          # WindowLeft, WindowRight
    h += s(0) + s(active_h - 1)          # WindowBottom, WindowTop
    h += s(0) + s(active_w - 1)          # ActiveLeft, ActiveRight
    h += s(0) + s(active_h - 1)          # ActiveBottom, ActiveTop
    h += s(0)                            # FrameNumber
    h += s(chan_type)                    # ColorChannelType
    h += s(nchan) + s(nmatte) + s(naux)  # channel counts
    h += s(struct.unpack('>h', struct.pack('>H', revision))[0])
    h += c(16, b'1.0')                   # Gamma
    h += c(24) + c(24) + c(24) + c(24)   # Red/Green/Blue chroma, WhitePoint
    h += l(0)                            # JobNumber
    h += c(128) + c(128)                 # FileName, Description
    h += c(64) + c(32) + c(32)           # ProgramName, MachineName, UserName
    h += c(20)                           # DateCreated
    h += c(24) + c(8)                    # Aspect, AspectRatio
    h += c(32)                           # ColorChannel
    h += s(field_rendered)
    h += c(12) + c(32)                   # Time, Filter
    h += s(chan_bits)
    h += s(matte_type) + s(matte_bits)
    h += s(aux_type) + s(aux_bits)
    h += c(32) + c(36)                   # AuxData, Reserved
    h += l(next_offset)                  # NextOffset
    assert len(h) == 740, len(h)
    return h


def scanline(width, nchan_total, chan_bytes=1):
    """One scanline record: per channel a uint16 length plus its RLE payload."""
    out = b''
    for _ in range(nchan_total):
        for _ in range(chan_bytes):
            payload = b''
            left = width
            while left > 0:
                n = min(left, 128)
                payload += bytes([n - 1, 0x40])  # n copies of 0x40
                left -= n
            out += struct.pack('>H', len(payload)) + payload
    return out


def build(subimages):
    """Concatenate subimages, resolving NextOffset and scanline offsets."""
    blobs = []
    for si in subimages:
        body = b''
        sot = []
        base = 740 + 4 * si['h']
        for _ in range(si['h']):
            sot.append(base + len(body))
            body += scanline(si['w'], si.get('nchan', 1))
        blobs.append((si, sot, body))

    offsets, pos = [], 0
    for si, sot, body in blobs:
        offsets.append(pos)
        pos += 740 + 4 * len(sot) + len(body)

    out = b''
    for i, (si, sot, body) in enumerate(blobs):
        nxt = offsets[i + 1] if i + 1 < len(blobs) else 0
        kw = {k: v for k, v in si.items() if k not in ('w', 'h', 'nchan')}
        hdr = header(si['w'], si['h'], nchan=si.get('nchan', 1),
                     next_offset=nxt, **kw)
        adj = [o + offsets[i] for o in sot]
        out += hdr + b''.join(struct.pack('>I', o) for o in adj) + body
    return out


def main(outdir):
    def write(name, data):
        with open(outdir + '/' + name, 'wb') as f:
            f.write(data)
        print('wrote %s (%d bytes)' % (name, len(data)))

    # H2: 1.2 KB claiming 65535 x 100 x 262 channels of uint32 = 6.8 GB.
    # Only the 400-byte offset table scales with the declared size, so the
    # ratio of declared to actual bytes is about 5.7 million.
    h = bytearray(header(8, 100, nchan=3, nmatte=3, naux=256,
                         chan_type=CT_DWORD, chan_bits=32,
                         matte_type=CT_DWORD, matte_bits=32,
                         aux_type=CT_DWORD, aux_bits=32))
    struct.pack_into('>hh', h, OFF_WINDOW_LEFT, -32768, 32766)
    struct.pack_into('>hh', h, OFF_ACTIVE_LEFT, -32768, 32766)
    sot = b''.join(struct.pack('>I', 740 + 400 + i * 16) for i in range(100))
    write('bomb.rla', bytes(h) + sot + b'\0' * 64)

    # H9: subimage 1's NextOffset points at itself, so walking the subimage
    # chain never terminates and the file appears to hold infinitely many
    # subimages.
    off1 = 740 + 4 * 8 + len(scanline(8, 1)) * 8
    d = bytearray(build([{'w': 8, 'h': 8}, {'w': 8, 'h': 8}]))
    struct.pack_into('>i', d, off1 + OFF_NEXT_OFFSET, off1)
    write('subimage-loop.rla', bytes(d))

    # Field-rendered: the offset table has one entry per scanline of the
    # active window but the height is halved, so table size and height
    # disagree by design. Must read cleanly rather than trip an assertion.
    write('field-rendered.rla', build([{'w': 8, 'h': 8, 'field_rendered': 1}]))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
