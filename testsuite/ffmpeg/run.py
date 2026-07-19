#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "vp9_display_p3.mkv", "vp9_rec2100_pq.mkv" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

# Regression test for narrow 10-bit VP9 input that used to make swscale write
# past OIIO's destination buffer during pixel reads.
command = command + oiiotool ("src/ffmpeg-width1-gbrp16.mkv --hash")
