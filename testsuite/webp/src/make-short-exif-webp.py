#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct


BASE = "base-short-exif.webp"
EXIF_FLAG = 0x08

CASES = [
    ("short-exif-len0.webp", b""),
    ("short-exif-len4.webp", b"Exif"),
    ("short-exif-len5.webp", b"Exif\x00"),
    ("short-exif-len6.webp", b"Exif\x00\x00"),
    ("short-exif-len13.webp", b"Exif\x00\x00II*\x00\x08\x00\x00"),
]


def read_chunks(data):
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise RuntimeError("%s is not a RIFF WebP file" % BASE)

    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise RuntimeError("truncated WebP chunk header")
        fourcc = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        begin = offset + 8
        end = begin + size
        if end > len(data):
            raise RuntimeError("truncated WebP chunk payload")
        chunks.append((fourcc, data[begin:end]))
        offset = end + (size & 1)
    return chunks


def write_chunk(fourcc, payload):
    chunk = fourcc + struct.pack("<I", len(payload)) + payload
    if len(payload) & 1:
        chunk += b"\x00"
    return chunk


def make_webp(exif_payload, image_chunks):
    vp8x_payload = bytes((EXIF_FLAG, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    chunks = write_chunk(b"VP8X", vp8x_payload)
    chunks += write_chunk(b"EXIF", exif_payload)
    for fourcc, payload in image_chunks:
        if fourcc not in (b"VP8X", b"EXIF"):
            chunks += write_chunk(fourcc, payload)
    body = b"WEBP" + chunks
    return b"RIFF" + struct.pack("<I", len(body)) + body


with open(BASE, "rb") as input_file:
    base_data = input_file.read()

image_chunks = read_chunks(base_data)

for filename, exif_payload in CASES:
    with open(filename, "wb") as output_file:
        output_file.write(make_webp(exif_payload, image_chunks))
