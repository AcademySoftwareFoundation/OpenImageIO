#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Test the "oiio:SourcePath" and "oiio:SourceFileFormat" attributes, which record
# which file an image was read from and which format reader read it, and
# check that no writer stores them in a file.

from __future__ import annotations

import numpy as np
import OpenImageIO as oiio


def check(desc: str, cond: bool) -> None:
    print(("ok   " if cond else "FAIL ") + desc)
    if not cond:
        raise SystemExit("FAILED: " + desc)


def source_of(spec: oiio.ImageSpec) -> tuple[str, str]:
    return (spec.get_string_attribute("oiio:SourcePath"),
            spec.get_string_attribute("oiio:SourceFileFormat"))


# The test makes its own input files. The name of the first one is also the
# marker the write check looks for in every written file.
SRC = "provenance-canary.tif"
TWO = "two-subimages.tif"
MARK = "provenance-canary"

src = oiio.ImageBuf(oiio.ImageSpec(32, 32, 3, "uint8"))
oiio.ImageBufAlgo.fill(src, (0.25, 0.5, 0.75))
check("wrote the source image", src.write(SRC))

spec = oiio.ImageSpec(4, 4, 3, "uint8")
pixels = np.zeros((4, 4, 3), dtype=np.uint8)
out = oiio.ImageOutput.create(TWO)
ok = out.open(TWO, [spec, spec]) and out.write_image(pixels)
ok = ok and out.open(TWO, spec, "AppendSubimage") and out.write_image(pixels)
check("wrote the two-subimage image", ok and out.close())

# ImageBuf read directly from the file: the spec carries what the ImageBuf
# itself reports.
buf = oiio.ImageBuf(SRC)
check("ImageBuf: source recorded", source_of(buf.spec()) == (SRC, "tiff"))
check("ImageBuf: agrees with name and file_format_name",
      source_of(buf.spec()) == (buf.name, buf.file_format_name))
check("ImageBuf: also on the native spec",
      source_of(buf.nativespec()) == (SRC, "tiff"))
check("ImageBuf, subimage 1: source recorded",
      source_of(oiio.ImageBuf(TWO, 1, 0).spec()) == (TWO, "tiff"))

# ImageCache, and an ImageBuf backed by it.
ic = oiio.ImageCache()
check("ImageCache: source recorded",
      source_of(ic.get_imagespec(SRC)) == (SRC, "tiff"))
check("ImageCache, subimage 1: source recorded",
      source_of(ic.get_imagespec(TWO, 1)) == (TWO, "tiff"))
oiio.attribute("imagebuf:use_imagecache", 1)
check("cache-backed ImageBuf: source recorded",
      source_of(oiio.ImageBuf(SRC).spec()) == (SRC, "tiff"))
oiio.attribute("imagebuf:use_imagecache", 0)

# An ImageBufAlgo result keeps the attributes, although the new ImageBuf was
# not itself read from a file.
resized = oiio.ImageBufAlgo.resize(buf, roi=oiio.ROI(0, 4, 0, 4, 0, 1, 0, 3))
check("ImageBufAlgo result: source kept",
      source_of(resized.spec()) == (SRC, "tiff"))
check("ImageBufAlgo result: its own file_format_name is empty",
      resized.file_format_name == "")

# No writer stores the source in a file. Write the image read above with
# every writer this build has, after replacing its format name with the
# marker too, then look for the marker in the file's bytes and in every
# attribute an ImageInput finds in it.
buf.specmod().attribute("oiio:SourceFileFormat", MARK + "-format")
gray = oiio.ImageBufAlgo.channels(buf, (0,))
extensions = {}
for entry in oiio.get_string_attribute("extension_list").split(";"):
    fmt, _, exts = entry.partition(":")
    extensions[fmt] = exts.split(",")[0]
failures = []
for fmt in oiio.get_string_attribute("output_format_list").split(","):
    if fmt in ("null", "term"):
        continue  # these write no file
    filename = "written." + extensions[fmt]
    # zfile writes only one channel; try the full image first.
    if not buf.write(filename):
        buf.geterror()
        if not gray.write(filename):
            failures.append(f"{fmt}: {gray.geterror()}")
            continue
    with open(filename, "rb") as f:
        if MARK.encode() in f.read():
            failures.append(f"{fmt}: {filename} contains the source")
    inp = oiio.ImageInput.open(filename)
    if not inp:
        failures.append(f"{fmt}: {oiio.geterror()}")
        continue
    for p in inp.spec().extra_attribs:
        if MARK in str(p.value):
            failures.append(f"{fmt}: {filename} has {p.name} = {p.value}")
    inp.close()
for failure in failures:
    print("FAIL " + failure)
check("no written file contains the source", not failures)

print("done.")
