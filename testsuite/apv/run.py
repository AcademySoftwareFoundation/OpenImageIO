#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# APV is lossy, so round trips are verified with oiiotool --diff and
# generous thresholds rather than exact reference images.

# A smooth gradient survives 4:2:2 chroma subsampling well.
command += oiiotool ("--pattern fill:topleft=0.1,0.1,0.1:topright=0.9,0.2,0.1:bottomleft=0.1,0.7,0.2:bottomright=0.2,0.2,0.9 128x96 3 -d uint16 -o pattern.tif")

# Default profile (422-10) write, then verify the metadata we expect.
command += oiiotool ("pattern.tif --attrib apv:qp 8 -o out422.apv")
command += info_command ("out422.apv", safematch=True)
command += oiiotool ("pattern.tif out422.apv --fail 0.05 --warn 0.02 --diff")

# 4:4:4 write / read round trip.
command += oiiotool ("pattern.tif --attrib apv:profile 444-10 --attrib apv:qp 5 -o out444.apv")
command += oiiotool ("pattern.tif out444.apv --fail 0.05 --warn 0.02 --diff")

# 12-bit 4:4:4.
command += oiiotool ("pattern.tif --attrib apv:profile 444-12 --attrib apv:qp 5 -o out444-12.apv")
command += info_command ("out444-12.apv", safematch=True)
command += oiiotool ("pattern.tif out444-12.apv --fail 0.05 --warn 0.02 --diff")

# RGBA via the 4444 profile.
command += oiiotool ("--pattern fill:topleft=0.1,0.1,0.1,1:topright=0.9,0.2,0.1,0.8:bottomleft=0.1,0.7,0.2,0.5:bottomright=0.2,0.2,0.9,0.2 64x64 4 -d uint16 --attrib apv:qp 5 -o out4444.apv")
command += info_command ("out4444.apv", safematch=True)

# Grayscale via the 400 profile.
command += oiiotool ("--pattern fill:topleft=0.05:topright=0.95:bottomleft=0.5:bottomright=0.5 64x64 1 -d uint16 -o out400.apv")
command += info_command ("out400.apv", safematch=True)

# Multiple subimages become multiple access units (frames).
command += oiiotool ("pattern.tif pattern.tif --mulc 0.5,0.5,0.5 --siappendall -d uint16 -o multi.apv")
command += info_command ("multi.apv", safematch=True)

# CICP carries through: tag as BT.2020/PQ, expect it back on read.
command += oiiotool ("pattern.tif --attrib:type=int[4] CICP 9,16,0,1 --attrib apv:qp 5 -o cicp.apv")
command += info_command ("cicp.apv", safematch=True)

# CICP derived from a color interop ID (no explicit CICP attribute):
# writing tags the bitstream, reading recovers both CICP and the ID.
command += oiiotool ("pattern.tif --attrib oiio:ColorSpace srgb_rec709_display --attrib apv:qp 5 -o ciid.apv")
command += info_command ("ciid.apv", safematch=True)
