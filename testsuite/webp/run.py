#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
    # We should also test read/write, but it's really hard to because it's
    # a lossy format and is not stable under the round trip
    # command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f,
    #                        extraargs='-attrib compression lossless')

command += oiiotool ("--create 64x64 4 -o rgba.webp")
command += info_command ("rgba.webp", "--iconfig oiio:UnassociatedAlpha 1", safematch=True)

# Resolution metadata is written as an EXIF chunk and read back
command += oiiotool ("--create 4x4 3 -d uint8 --attrib:type=float XResolution 150 --attrib:type=float YResolution 150 --attrib ResolutionUnit in -o resolution.webp")
command += info_command ("resolution.webp", safematch=True)
# Integer resolution, as oiiotool --attrib stores "72"
command += oiiotool ("--create 4x4 3 -d uint8 --attrib XResolution 72 --attrib YResolution 72 --attrib ResolutionUnit cm -o resolution-int.webp")
command += info_command ("resolution-int.webp", safematch=True)


command += oiiotool ("--create 1x1 3 -d uint8 -o base-short-exif.webp")
command += run_app (pythonbin + " src/make-short-exif-webp.py", silent=True)
short_exif_files = [
    "short-exif-len0.webp",
    "short-exif-len4.webp",
    "short-exif-len5.webp",
    "short-exif-len6.webp",
    "short-exif-len13.webp",
]
for f in short_exif_files:
    command += iconvert(f"{f} out.null && echo {f}-ok")

# Regression test: a 76-byte WebP whose VP8X header declares a 16383x16383
# canvas inconsistent with its tiny frame. Must be rejected cleanly (libwebp's
# demux catches the mismatch; the open-time check_open/compression-ratio guard
# now runs before the decoded-image allocation regardless).
command += info_command ("src/bomb-16383.webp", failureok=True, safematch=True)
