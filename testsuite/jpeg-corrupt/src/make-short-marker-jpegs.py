#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from pathlib import Path


APP1 = 0xE1
APP2 = 0xE2
BASE = Path("base-short-marker.jpg")


def marker(marker_id, payload):
    length = len(payload) + 2
    return b"\xff" + bytes([marker_id]) + length.to_bytes(2, "big") + payload


def write_with_marker(name, marker_id, payload):
    data = BASE.read_bytes()
    if data[:2] != b"\xff\xd8":
        raise RuntimeError(f"{BASE} is not a JPEG stream")
    Path(name).write_bytes(data[:2] + marker(marker_id, payload) + data[2:])


def exif_payload(thumbnail):
    # A valid little-endian Exif TIFF header with an empty IFD0, so that the
    # Exif decode itself succeeds and the thumbnail scan is what's tested.
    tiff = (b"II*\0" + (8).to_bytes(4, "little") + (0).to_bytes(2, "little")
            + (0).to_bytes(4, "little"))
    return b"Exif\0\0" + tiff + thumbnail


write_with_marker("short-exif-app1-len4.jpg", APP1, b"Exif")
write_with_marker("short-exif-app1-len5.jpg", APP1, b"Exif\0")
write_with_marker("short-icc-app2-len11.jpg", APP2, b"ICC_PROFILE")
write_with_marker("short-icc-app2-len12.jpg", APP2, b"ICC_PROFILE\0")
write_with_marker("short-icc-app2-len13.jpg", APP2, b"ICC_PROFILE\0\1")

# Thumbnail whose SOF0 declares a segment length of 0. The length counts its
# own two bytes, so subtracting them underflowed the 16 bit value into a
# ~64 KB skip.
write_with_marker("thumbnail-sof-len0.jpg", APP1,
                  exif_payload(b"\xff\xd8\xff\xc0\x00\x00\x08\x00\x10\x00\x10"
                               b"\x03\xff\xd9"))

# Thumbnail whose DQT declares a segment length running past the end of the
# Exif block.
write_with_marker("thumbnail-dqt-len-overflow.jpg", APP1,
                  exif_payload(b"\xff\xd8\xff\xdb\xff\xff" + b"\0" * 8
                               + b"\xff\xd9"))
