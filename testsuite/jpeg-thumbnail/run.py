#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os
OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR')

redirect = " >> out.txt 2>&1 "

command += oiiotool ( OIIO_TESTSUITE_IMAGEDIR + '/tahoe-gps.jpg --thumbnail-get --eraseattrib ".*" --printinfo' )
outputs = [ "out.txt" ]
