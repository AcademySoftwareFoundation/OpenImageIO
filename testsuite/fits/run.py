#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# ../fits-image/pg93:
# tst0001.fits to tst0014.fits
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/pg93"
files = [ "tst0001.fits", "tst0003.fits",
           "tst0005.fits", "tst0006.fits", "tst0007.fits",  "tst0008.fits",
           #FIXME? "tst0009.fits",
          "tst0013.fits"
        ]
for f in files :
    command += rw_command (imagedir, f)

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/ftt4b"
files = [ "file001.fits", "file002.fits", "file003.fits",
          "file009.fits", "file012.fits" ]
for f in files :
    command += rw_command (imagedir, f)
