#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# save the error output
redirect = " >> out.txt 2>&1 "


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
          "file006.fits", "file009.fits", "file012.fits" ]
for f in files :
    command += rw_command (imagedir, f)

command += info_command ("src/rgb.fits")
command += rw_command ("src", "rgb.fits")

# Regression tests for broken files
command += info_command ("src/broken_no_END.fits", verbose=False, failureok=True)
# NAXIS declares 999999999 axes: must be rejected before sizing the axis
# vector (previously an unvalidated resize -> OOM/crash).
command += info_command ("src/bad-naxis.fits", verbose=False, failureok=True)
# A 2880-byte header declaring a ~1.5 GB image (40000x40000): the
# compression-ratio guard must reject it before the caller allocates.
command += info_command ("src/bomb-40000.fits", verbose=False, failureok=True)

outputs = [ "out.txt" ]
