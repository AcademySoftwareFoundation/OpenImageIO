#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Opening an image must not load a color config: a reader records what the
# file says, and interpreting that through a config is up to the caller (see
# also issues #4629 and #5490). Loading a config is logged as
# "ColorConfig::reset" in the timing report.

from __future__ import annotations

import os
import struct
import sys

import OpenImageIO as oiio

# ICO reads PNG subimages with the PNG reader: wrap the PNG (which has a gAMA
# chunk) in a one-entry 32x32 ICO.
if os.path.exists("colorconfig.png"):
    png = open("colorconfig.png", "rb").read()
    with open("colorconfig.ico", "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, 1))
        f.write(struct.pack("<BBBBHHII", 32, 32, 0, 0, 1, 32, len(png), 22))
        f.write(png)

oiio.attribute("log_times", 1)


def config_loaded() -> bool:
    return "ColorConfig" in oiio.get_string_attribute("timing_report")


for filename in sys.argv[1:]:
    probe = oiio.ImageInput.create(filename)
    if probe is None:
        oiio.geterror()
        continue  # no reader for this format in this build
    # OpenEXR has two readers, selected by "openexr:core"
    for core in (0, 1) if filename.endswith(".exr") else (1,):
        oiio.attribute("openexr:core", core)
        inp = oiio.ImageInput.open(filename)
        if inp is None:
            print("could not open", filename, oiio.geterror())
            continue
        labels = []
        while inp.seek_subimage(len(labels), 0):
            labels.append(inp.spec().getattribute("oiio:ColorSpace"))
        inp.close()
        # Show that the color paths these files are here for were read
        if filename.endswith("-chrm.png"):
            data = open(filename, "rb").read()
            print("PNG has gAMA and cHRM:",
                  b"gAMA" in data and b"cHRM" in data)
        elif filename.endswith(".exr"):
            print(os.path.basename(filename), "core", core, "labels:", *labels)
    if config_loaded():
        print("reading", filename, "loaded a color config")
        break

print("Reading loaded a color config:", config_loaded())
oiio.ColorConfig()
print("Constructing a ColorConfig is detected:", config_loaded())
