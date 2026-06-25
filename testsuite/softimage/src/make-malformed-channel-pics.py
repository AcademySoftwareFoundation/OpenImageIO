#!/usr/bin/env python3

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct


MAGIC = 0x5380F634
WIDTH = 2
HEIGHT = 1
RED_CHANNEL = 0x80
GREEN_CHANNEL = 0x40

UNCOMPRESSED = 0
PURE_RUN_LENGTH = 1
MIXED_RUN_LENGTH = 2


def header():
    comment = b"Malformed channel packet regression"
    return struct.pack(">If80s4sHHfHH", MAGIC, 1.0, comment, b"PICT",
                       WIDTH, HEIGHT, 1.0, 0, 0)


def channel_packet(chained, size, encoding, channel):
    return bytes((chained, size, encoding, channel))


def write_pic(filename, encoding, red_payload, green_payload):
    with open(filename, "wb") as out:
        out.write(header())
        out.write(channel_packet(1, 16, encoding, RED_CHANNEL))
        out.write(channel_packet(0, 8, encoding, GREEN_CHANNEL))
        out.write(red_payload)
        out.write(green_payload)


write_pic("inconsistent-bpc-uncompressed.pic", UNCOMPRESSED,
          b"\x00\x10\x00\x20", b"\x30\x40")
write_pic("inconsistent-bpc-pure-rle.pic", PURE_RUN_LENGTH,
          b"\x02\x00\x10", b"\x02\x30")
write_pic("inconsistent-bpc-mixed-rle.pic", MIXED_RUN_LENGTH,
          b"\x01\x00\x10\x00\x20", b"\x01\x30\x40")
