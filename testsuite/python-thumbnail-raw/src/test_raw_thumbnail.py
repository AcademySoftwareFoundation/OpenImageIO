#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import OpenImageIO as oiio

import os

OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR')

######################################################################
# main test starts here


test_files = [
        'RAW_CANON_EOS_7D.CR2',
        'RAW_FUJI_F700.RAF',
        'RAW_NIKON_D3X.NEF',
        'RAW_OLYMPUS_E3.ORF',
        'RAW_PANASONIC_DMC-GF1.RW2',
        'RAW_PANASONIC_G1.RW2',
        'RAW_PENTAX_K200D.PEF',
        'RAW_SONY_A300.ARW'
]

def read_thumbnail(filename, index = -1, sort = False):
    
    hint = oiio.ImageSpec()
    
    if index != -1:
        hint['raw:thumbnail_index'] = index
    if sort:
        hint['raw:thumbnail_sort'] = True
    
    input = oiio.ImageInput.open (OIIO_TESTSUITE_IMAGEDIR + '/' + file, hint)
    thumbnail = input.get_thumbnail()
    print('  Thumbnail index:', index, 'Sort:', sort)
    print('    ', thumbnail.spec().width)
    print('    ', thumbnail.spec().height)
    print('    ', thumbnail.spec().nchannels)

try:
    for file in test_files:
        print(file)
        read_thumbnail(file, -1)
        read_thumbnail(file, 0)
        read_thumbnail(file, 1, True)
    
    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
