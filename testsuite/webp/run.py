#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
    # We should also test read/write, but it's really hard to because it's
    # a lossy format and is not stable under the round trip
    # command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f,
    #                        extraargs='-attrib compression lossless')

command += oiiotool ("--create 64x64 4 -o rgba.webp")
command += info_command ("rgba.webp", "--iconfig oiio:UnassociatedAlpha 1", safematch=True)


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
    command += iconvert(f + " out.null", successmessage=f + "-ok")
