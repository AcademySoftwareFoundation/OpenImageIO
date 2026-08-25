#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR')

######################################################################
# main test starts here

redirect = " >> out.txt 2>&1 "

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
    
    label = '{} index {} sort {}'.format(filename, index, sort)
    params = '--echo "' + label + '" '
    if index != -1:
        params += '-iconfig "raw:thumbnail_index" ' + str(index) + ' '
    if sort:
        params += '-iconfig "raw:thumbnail_sort" -1 '
        
    params += OIIO_TESTSUITE_IMAGEDIR + '/' + file
    params += ' --thumbnail-get --eraseattrib ".*" --printinfo'

    return params

for file in test_files:
    command += oiiotool (read_thumbnail(file, -1))
    command += oiiotool (read_thumbnail(file, 0))
    command += oiiotool (read_thumbnail(file, 1, True))

outputs = [ "out.txt" ]
