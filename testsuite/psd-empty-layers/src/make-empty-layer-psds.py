#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct
from pathlib import Path


def layer(width, height, compression, name, values=(32, 96, 160), hidden=False):
    encoded_name = name.encode("ascii")
    pascal_name = bytes([len(encoded_name)]) + encoded_name
    pascal_name += bytes(-len(pascal_name) % 4)
    extra = struct.pack(">II", 0, 0) + pascal_name
    channels = []
    for value in values:
        if not width or not height:
            data = struct.pack(">H", compression)
        else:
            data = struct.pack(">H", 0) + bytes([value]) * width * height
        channels.append(data)
    record = struct.pack(">iiiiH", 0, 0, height, width, 3)
    record += b"".join(
        struct.pack(">hI", channel, len(data))
        for channel, data in enumerate(channels)
    )
    record += b"8BIMnorm" + bytes([255, 0, 2 if hidden else 0, 0])
    record += struct.pack(">I", len(extra)) + extra
    return record, b"".join(channels)


def write_psd(filename, width, height, compression, canvas_width=2,
              hidden=False):
    header = b"8BPS" + struct.pack(">H6sHIIHH", 1, bytes(6), 3,
                                 2, canvas_width, 8, 3)
    records = [layer(1, 1, 0, "before"),
               layer(width, height, compression, "empty", hidden=hidden),
               layer(1, 1, 0, "after", values=(160, 96, 32))]
    layer_info = struct.pack(">h", len(records))
    layer_info += b"".join(record for record, data in records)
    layer_info += b"".join(data for record, data in records)
    layer_info += bytes(len(layer_info) % 2)
    layer_mask = struct.pack(">I", len(layer_info)) + layer_info
    layer_mask += struct.pack(">I", 0)
    composite = struct.pack(">H", 0)
    composite += b"".join(bytes([v]) * 4 for v in (32, 96, 160))
    Path(filename).write_bytes(
        header + struct.pack(">II", 0, 0)
        + struct.pack(">I", len(layer_mask)) + layer_mask + composite
    )


for compression in (0, 1):
    for width, height in ((0, 0), (0, 2), (2, 0)):
        write_psd(f"empty-layer-{width}x{height}-{compression}.psd",
                  width, height, compression)

for compression in (0, 1):
    write_psd(f"hidden-empty-layer-{compression}.psd", 0, 0, compression,
              hidden=True)
write_psd("hidden-populated-layer.psd", 1, 1, 0, hidden=True)

# An empty layer must not bypass the existing document and layer size limits.
write_psd("empty-layer-invalid-canvas.psd", 0, 0, 0, canvas_width=0)
write_psd("empty-layer-oversized.psd", 0, 30001, 0)
