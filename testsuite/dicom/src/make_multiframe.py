#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Write a tiny multi-frame DICOM file, explicit VR little endian, so the
# testsuite can exercise reading a DICOM with more than one frame without
# needing an external image. Each frame is a constant gray value 10 frames
# apart, so a per-subimage hash tells us whether the right frame was read.

import struct
import sys

WIDTH = 4
HEIGHT = 3
FRAMES = 3

SOP_CLASS = b'1.2.840.10008.5.1.4.1.1.7'      # Secondary Capture
SOP_INSTANCE = b'1.2.826.0.1.3680043.2.1125.1.1'
EXPLICIT_VR_LE = b'1.2.840.10008.1.2.1'

# VRs whose length field is 32 bits, following a 2-byte reserved field.
LONG_VRS = (b'OB', b'OW', b'OF', b'SQ', b'UT', b'UN')


def element(group, elem, vr, value):
    # DICOM values are padded to an even length: text with a space, binary
    # and UIDs with a null.
    if len(value) % 2:
        value += b' ' if vr in (b'CS', b'IS', b'DS') else b'\x00'
    if vr in LONG_VRS:
        return struct.pack('<HH2sHI', group, elem, vr, 0, len(value)) + value
    return struct.pack('<HH2sH', group, elem, vr, len(value)) + value


def main(filename):
    pixels = b''.join(bytes([10 * (f + 1)]) * (WIDTH * HEIGHT)
                      for f in range(FRAMES))

    dataset = b''.join([
        element(0x0008, 0x0016, b'UI', SOP_CLASS),
        element(0x0008, 0x0018, b'UI', SOP_INSTANCE),
        element(0x0008, 0x0060, b'CS', b'OT'),          # Modality
        element(0x0020, 0x0013, b'IS', b'1'),           # InstanceNumber
        element(0x0028, 0x0002, b'US', struct.pack('<H', 1)),
        element(0x0028, 0x0004, b'CS', b'MONOCHROME2'),
        element(0x0028, 0x0008, b'IS', str(FRAMES).encode()),
        element(0x0028, 0x0010, b'US', struct.pack('<H', HEIGHT)),
        element(0x0028, 0x0011, b'US', struct.pack('<H', WIDTH)),
        element(0x0028, 0x0100, b'US', struct.pack('<H', 8)),
        element(0x0028, 0x0101, b'US', struct.pack('<H', 8)),
        element(0x0028, 0x0102, b'US', struct.pack('<H', 7)),
        element(0x0028, 0x0103, b'US', struct.pack('<H', 0)),
        element(0x7FE0, 0x0010, b'OB', pixels),
    ])

    meta = b''.join([
        element(0x0002, 0x0001, b'OB', b'\x00\x01'),
        element(0x0002, 0x0002, b'UI', SOP_CLASS),
        element(0x0002, 0x0003, b'UI', SOP_INSTANCE),
        element(0x0002, 0x0010, b'UI', EXPLICIT_VR_LE),
    ])
    meta = element(0x0002, 0x0000, b'UL', struct.pack('<I', len(meta))) + meta

    with open(filename, 'wb') as f:
        f.write(b'\x00' * 128 + b'DICM' + meta + dataset)


main(sys.argv[1])
