#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# CICP (ITU-T H.273) is transport metadata: it may ride only in a format's
# native CICP slot (PNG's cICP chunk). A writer that declares no such slot via
# supports("cicp") -- e.g. OpenEXR -- must strip it on write so it never leaks
# into the container as a generic custom attribute. This locks both halves:
# EXR strips CICP; PNG still emits the cICP chunk (round-trip preserved).

from __future__ import annotations

import OpenImageIO as oiio


def check(desc, cond):
    print(("ok   " if cond else "FAIL ") + desc)
    if not cond:
        raise SystemExit("FAILED: " + desc)


CICP = (1, 13, 0, 1)


def make_buf(basetype):
    spec = oiio.ImageSpec(4, 4, 3, basetype)
    spec.attribute("CICP", oiio.TypeDesc("int[4]"), CICP)
    return oiio.ImageBuf(spec)


# EXR has no native CICP slot: the attribute must be gone from the on-disk
# header. Read back with a bare ImageInput, which reports only what the plugin
# actually parsed from the file.
make_buf(oiio.FLOAT).write("out.exr")
exr_in = oiio.ImageInput.open("out.exr")
check("written EXR: CICP is stripped (no native slot)",
      exr_in.spec().getattribute("CICP") is None)
exr_in.close()

# PNG has a native cICP chunk: the tuple must survive the write.
make_buf(oiio.UINT8).write("out.png")
png_in = oiio.ImageInput.open("out.png")
png_cicp = png_in.spec().getattribute("CICP")
check("written PNG: cICP chunk still emitted",
      png_cicp is not None and tuple(png_cicp) == CICP)
png_in.close()

print("done.")
