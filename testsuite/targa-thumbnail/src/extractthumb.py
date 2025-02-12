#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import OpenImageIO as oiio

import sys

if len(sys.argv) < 3 :
    print("No filenames specified")
    exit(1)

inname = sys.argv[1]
outname = sys.argv[2]
buf = oiio.ImageBuf(inname)
if buf.has_thumbnail :
    th = buf.get_thumbnail()
    th.write(outname, dtype='uint8')
