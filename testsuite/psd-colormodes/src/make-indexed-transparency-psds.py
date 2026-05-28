#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct


BASE = "src/pattern2-8-indexed.psd"
RESOURCE_TRANSPARENCY_INDEX = 1047

CASES = [
    ("indexed-transparency-0.psd", 0),
    ("indexed-transparency-255.psd", 255),
    ("indexed-transparency-256.psd", 256),
]


def resource_section_bounds(data):
    offset = 26
    color_data_length = struct.unpack_from(">I", data, offset)[0]
    offset += 4 + color_data_length
    resource_length = struct.unpack_from(">I", data, offset)[0]
    offset += 4
    return offset, offset + resource_length


def find_transparency_index_resource(data):
    offset, end = resource_section_bounds(data)
    while offset < end:
        if offset + 12 > end:
            raise RuntimeError("truncated image resource")
        if data[offset : offset + 4] != b"8BIM":
            raise RuntimeError("unexpected image resource signature")

        resource_id = struct.unpack_from(">H", data, offset + 4)[0]
        name_length = data[offset + 6]
        name_size = 1 + name_length
        if name_size & 1:
            name_size += 1
        data_length_offset = offset + 6 + name_size
        data_length = struct.unpack_from(">I", data, data_length_offset)[0]
        data_offset = data_length_offset + 4

        if data_offset + data_length > end:
            raise RuntimeError("truncated image resource payload")
        if resource_id == RESOURCE_TRANSPARENCY_INDEX:
            if data_length != 2:
                raise RuntimeError("unexpected transparency-index length")
            return data_offset

        offset = data_offset + data_length
        if data_length & 1:
            offset += 1

    raise RuntimeError("transparency-index resource not found")


with open(BASE, "rb") as input_file:
    base_data = input_file.read()

index_offset = find_transparency_index_resource(base_data)

for filename, transparency_index in CASES:
    data = bytearray(base_data)
    struct.pack_into(">H", data, index_offset, transparency_index)
    with open(filename, "wb") as output_file:
        output_file.write(data)
