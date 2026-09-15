#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os
OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR')

redirect = " >> out.txt 2>&1 "

command += oiiotool ( OIIO_TESTSUITE_IMAGEDIR + '/tahoe-gps.jpg --thumbnail-get --eraseattrib ".*" --printinfo' )

# The thumbnail's marker segments are big endian no matter what byte order
# the enclosing Exif block uses. tahoe-gps.jpg is "MM", sky.jpg is "II"; both
# must report the thumbnail's true dimensions.
command += oiiotool ( OIIO_TESTSUITE_IMAGEDIR + '/tahoe-gps.jpg --echo "tahoe-gps thumbnail {TOP.thumbnail_width}x{TOP.thumbnail_height}x{TOP.thumbnail_nchannels}"' )
command += oiiotool ( OIIO_TESTSUITE_IMAGEDIR + '/jpeg/ultrahdr/sky.jpg --echo "sky thumbnail {TOP.thumbnail_width}x{TOP.thumbnail_height}x{TOP.thumbnail_nchannels}"' )

outputs = [ "out.txt" ]
