#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Exercise the P6b source-provenance attributes: "oiio:SourceFormat" and
# "oiio:SourcePath". See src/doc/stdmetadata.rst.

from __future__ import annotations

import OpenImageIO as oiio


SRC_TIFF = "../common/tahoe-tiny.tif"
SRC_EXR  = "../common/checker_with_alpha.exr"


def check(desc, cond):
    print(("ok   " if cond else "FAIL ") + desc)
    if not cond:
        raise SystemExit("FAILED: " + desc)


# 1. Plain ImageBuf read (no explicit ImageCache): the reader deposits both
#    attributes on the spec, mirroring what ImageBuf::name() /
#    file_format_name() already report live for this instance.
buf = oiio.ImageBuf(SRC_TIFF)
spec = buf.spec()
check("plain ImageBuf: oiio:SourceFormat == 'tiff'",
      spec.get_string_attribute("oiio:SourceFormat") == "tiff")
check("plain ImageBuf: oiio:SourcePath == filename",
      spec.get_string_attribute("oiio:SourcePath") == SRC_TIFF)
check("plain ImageBuf: matches live file_format_name()",
      spec.get_string_attribute("oiio:SourceFormat") == buf.file_format_name)
check("plain ImageBuf: matches live name",
      spec.get_string_attribute("oiio:SourcePath") == buf.name)

# 2. ImageCache-backed read: this is the path the recon found *drops*
#    ImageBuf::name()/file_format_name() information today. Confirm the
#    spec attributes survive it.
ic = oiio.ImageCache()
ic_spec = ic.get_imagespec(SRC_TIFF)
check("ImageCache: oiio:SourceFormat == 'tiff'",
      ic_spec.get_string_attribute("oiio:SourceFormat") == "tiff")
check("ImageCache: oiio:SourcePath == filename",
      ic_spec.get_string_attribute("oiio:SourcePath") == SRC_TIFF)

# 3. Same, but via a plain ImageBuf that is (transparently) backed by the
#    shared ImageCache, exactly the "dropped by ImageCache" case the recon
#    identified.
oiio.attribute("imagebuf:use_imagecache", 1)
buf_ic = oiio.ImageBuf(SRC_TIFF)
spec_ic = buf_ic.spec()
check("cache-backed ImageBuf: oiio:SourceFormat == 'tiff'",
      spec_ic.get_string_attribute("oiio:SourceFormat") == "tiff")
check("cache-backed ImageBuf: oiio:SourcePath == filename",
      spec_ic.get_string_attribute("oiio:SourcePath") == SRC_TIFF)
oiio.attribute("imagebuf:use_imagecache", 0)

# 4. IBA boundary: run an ImageBufAlgo op that produces a brand new ImageBuf.
#    The result's own name()/file_format_name() are empty/meaningless (it
#    was never itself read from a file), but the source-provenance
#    attributes on its spec (copied from the source spec) still identify
#    where the pixels originally came from.
resized = oiio.ImageBufAlgo.resize(buf, roi=oiio.ROI(0, 32, 0, 32, 0, 1, 0, 3))
rspec = resized.spec()
check("post-IBA: oiio:SourceFormat survives IBA op",
      rspec.get_string_attribute("oiio:SourceFormat") == "tiff")
check("post-IBA: oiio:SourcePath survives IBA op",
      rspec.get_string_attribute("oiio:SourcePath") == SRC_TIFF)
check("post-IBA: the result's own file_format_name is NOT set (would be if "
      "this were duplicating a live accessor instead of filling the gap)",
      resized.file_format_name == "")

# 5. Write policy: "oiio:SourcePath" must never leak into a written file by
#    default (privacy: no embedded local path). Verify against the file's
#    own on-disk metadata -- read it back with a bare ImageInput, which
#    reports only what the plugin actually parsed from the file, not a
#    fresh re-deposit. (Both attributes are internal "oiio:*" metadata, and
#    every writer already suppresses those by default; a future write-policy
#    attribute may offer to preserve "oiio:SourceFormat" -- see
#    stdmetadata.rst.)
src = oiio.ImageBuf(SRC_EXR)
src.write("out.exr")

raw_in = oiio.ImageInput.open("out.exr")
raw_spec = raw_in.spec()
check("written EXR: oiio:SourcePath is stripped by default",
      raw_spec.get_string_attribute("oiio:SourcePath") == "")
raw_in.close()

print("done.")
