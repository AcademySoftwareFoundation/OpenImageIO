#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct
import zlib
from pathlib import Path


def layer(width, height, compression, name, values=(32, 96, 160), hidden=False,
          mask=False, version=1, data_length_extra=0):
    encoded_name = name.encode("ascii")
    pascal_name = bytes([len(encoded_name)]) + encoded_name
    pascal_name += bytes(-len(pascal_name) % 4)
    mask_data = struct.pack(">iiiiBB2x", 0, 0, 2, 2, 255, 0) if mask else b""
    extra = struct.pack(">I", len(mask_data)) + mask_data
    extra += struct.pack(">I", 0) + pascal_name
    channels = []
    for value in values:
        if not width or not height:
            data = struct.pack(">H", compression)
            if compression == 1:
                data += bytes(height * (2 if version == 1 else 4))
            elif compression == 2:
                data += zlib.compress(b"")
        else:
            data = struct.pack(">H", 0) + bytes([value]) * width * height
        channels.append((len(channels), data))
    if mask:
        channels.append((-2, struct.pack(">H", 0) + bytes([255]) * 4))
    record = struct.pack(">iiiiH", 0, 0, height, width, len(channels))
    record += b"".join(
        struct.pack(">hI" if version == 1 else ">hQ", channel,
                    len(data) + data_length_extra)
        for channel, data in channels
    )
    record += b"8BIMnorm" + bytes([255, 0, 2 if hidden else 0, 0])
    record += struct.pack(">I", len(extra)) + extra
    return record, b"".join(data for channel, data in channels)


def write_psd(filename, width, height, compression, canvas_width=2,
              hidden=False, records=None, version=1):
    header = b"8BPS" + struct.pack(">H6sHIIHH", version, bytes(6), 3,
                                 2, canvas_width, 8, 3)
    if records is None:
        records = [layer(1, 1, 0, "before", version=version),
                   layer(width, height, compression, "empty", hidden=hidden,
                         version=version),
                   layer(1, 1, 0, "after", values=(160, 96, 32), version=version)]
    layer_info = struct.pack(">h", len(records))
    layer_info += b"".join(record for record, data in records)
    layer_info += b"".join(data for record, data in records)
    layer_info += bytes(len(layer_info) % 2)
    length_format = ">I" if version == 1 else ">Q"
    layer_mask = struct.pack(length_format, len(layer_info)) + layer_info
    layer_mask += struct.pack(">I", 0)
    composite = struct.pack(">H", 0)
    composite += b"".join(bytes([v]) * 4 for v in (32, 96, 160))
    Path(filename).write_bytes(
        header + struct.pack(">II", 0, 0)
        + struct.pack(length_format, len(layer_mask)) + layer_mask + composite
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
write_psd("empty-layer-oversized-width.psd", 30001, 0, 0)
write_psd("all-empty.psd", 0, 0, 0,
          records=[layer(0, 0, 0, "empty"), layer(0, 2, 1, "empty2")])
write_psd("trailing-empty.psd", 0, 0, 0,
          records=[layer(1, 1, 0, "before"), layer(0, 0, 0, "empty")])
write_psd("empty-layer-with-mask.psd", 0, 0, 0,
          records=[layer(1, 1, 0, "before"), layer(0, 0, 0, "empty", mask=True),
                   layer(1, 1, 0, "after", values=(160, 96, 32))])
write_psd("empty-layer-zip.psd", 0, 2, 2)
write_psd("empty-layer-truncated-channel.psd", 0, 2, 1,
          records=[layer(0, 2, 1, "empty", data_length_extra=1000000)])
for width, height in ((0, 300000), (300000, 0)):
    write_psd(f"empty-layer-{width}x{height}.psb", width, height, 0, version=2)
for width, height in ((0, 300001), (300001, 0)):
    write_psd(f"oversized-layer-{width}x{height}.psb", width, height, 0, version=2)
