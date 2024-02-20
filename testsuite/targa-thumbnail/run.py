#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import OpenImageIO as oiio
import os

redirect = " >> out.txt 2>> out.err.txt"

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/targa"

files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA",
          "round_grill.tga" ]


# Test ability to extract thumbnails
outputs = [ ]
for f in files:
    fin = imagedir + "/" + f
    buf = oiio.ImageBuf(fin)
    if buf.has_thumbnail :
        thumbname = f + ".tif"
        command += run_app(pythonbin + " src/extractthumb.py " + fin + " " + thumbname)
        outputs += [ thumbname ]

outputs += [ 'out.txt' ]
