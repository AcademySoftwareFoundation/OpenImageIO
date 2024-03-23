#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os

files = [ "RAW_CANON_EOS_7D.CR2",
          "RAW_NIKON_D3X.NEF",
          "RAW_FUJI_F700.RAF",
          "RAW_NIKON_D3X.NEF",
          "RAW_OLYMPUS_E3.ORF",
          "RAW_PANASONIC_G1.RW2",
          "RAW_PENTAX_K200D.PEF",
          "RAW_SONY_A300.ARW" ]
outputs = []

# Things vary a lot with libraw versions.
# FIXME -- return to this later
if (os.getenv('GITHUB_ACTIONS') == 'true'):
    failthresh = 0.024
    files.remove ("RAW_PANASONIC_G1.RW2")

# Fairly high hard fail, since libraw seems to diddle with its debayering
# from version to version, it's hard to make a single reference image.
hardfail = 0.017

# For each test image, read it and print all metadata, resize it (to make
# the ref images small) and compared to the reference.
for f in files:
    outputname = f+".tif"
    command += oiiotool ("-iconfig raw:ColorSpace linear "
                         + "-i:info=2 " + OIIO_TESTSUITE_IMAGEDIR + "/" + f
                         + " -resample '5%' -d uint8 "
                         + "-o " + outputname)
    outputs += [ outputname ]

outputs += [ "out.txt" ]
