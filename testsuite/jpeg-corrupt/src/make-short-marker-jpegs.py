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


write_with_marker("short-exif-app1-len4.jpg", APP1, b"Exif")
write_with_marker("short-exif-app1-len5.jpg", APP1, b"Exif\0")
write_with_marker("short-icc-app2-len11.jpg", APP2, b"ICC_PROFILE")
write_with_marker("short-icc-app2-len12.jpg", APP2, b"ICC_PROFILE\0")
write_with_marker("short-icc-app2-len13.jpg", APP2, b"ICC_PROFILE\0\1")
