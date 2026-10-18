#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


tool, root = sys.argv[1], Path(sys.argv[2])
composite = (2, 2, "", "6585CA1F6B702CF27768484219F6F9200F4E9192")
before = (1, 1, "before", "E3FF44389E80210E4545A907BE55980CEB6EA821")
after = (1, 1, "after", "5818BEB65A1338D7C80684EEC2EA2ECB0DB183D0")
populated = (1, 1, "empty", before[3])
cases = []
for compression in (0, 1):
    for width, height in ((0, 0), (0, 2), (2, 0)):
        cases.append((f"empty-layer-{width}x{height}-{compression}.psd",
                      [composite, before, after]))
    cases.append((f"hidden-empty-layer-{compression}.psd", [composite, before, after]))
cases += [
    ("hidden-populated-layer.psd", [composite, before, populated, after]),
    ("all-empty.psd", [composite]),
    ("trailing-empty.psd", [composite, before]),
    ("empty-layer-with-mask.psd", [composite, before, after]),
    ("empty-layer-zip.psd", [composite, before, after]),
    ("empty-layer-0x300000.psb", [composite, before, after]),
    ("empty-layer-300000x0.psb", [composite, before, after]),
]

for raw in (0, 1):
    for filename, expected in cases:
        result = subprocess.run(
            [tool, "--iconfig", "psd:RawData", str(raw),
             "--info:format=xml:verbose=1", "-a", "--hash", str(root / filename)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        assert result.returncode == 0, (filename, raw, result.stderr)
        specs = ET.fromstring("<images>" + result.stdout + "</images>")
        actual = [
            (int(spec.findtext("width")), int(spec.findtext("height")),
             spec.findtext("attrib[@name='oiio:subimagename']", ""),
             spec.findtext("SHA1"))
            for spec in specs
        ]
        assert actual == expected, (filename, raw, actual, expected)
        assert all(int(spec.findtext("subimages", "1")) == len(expected)
                   for spec in specs), (filename, raw, "incorrect subimage count")

for filename, diagnostic in (
    ("empty-layer-invalid-canvas.psd", "[Header]"),
    ("empty-layer-oversized.psd", "[Layer Record]"),
    ("empty-layer-oversized-width.psd", "[Layer Record]"),
    ("oversized-layer-0x300001.psb", "[Layer Record]"),
    ("oversized-layer-300001x0.psb", "[Layer Record]"),
    ("empty-layer-truncated-channel.psd", "[Layer Channel]"),
):
    result = subprocess.run(
        [tool, "--info", str(root / filename)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    assert result.returncode != 0 and diagnostic in result.stderr, (
        filename, result.returncode, result.stderr)

print("Empty-layer subimages, populated pixels, raw mode and size limits verified.")
