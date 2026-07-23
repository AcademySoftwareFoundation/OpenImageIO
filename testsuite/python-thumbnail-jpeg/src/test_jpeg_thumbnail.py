#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import OpenImageIO as oiio

import os

OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR',
                                    '../oiio-images')

######################################################################
# main test starts here

try:
    input = oiio.ImageInput.open (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
    thumbnail = input.get_thumbnail()
    assert thumbnail is not None
    assert thumbnail.spec().width == 320
    assert thumbnail.spec().height == 240
    assert thumbnail.spec().nchannels == 3
    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
