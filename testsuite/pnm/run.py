#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/pnm"

for f in [ "bw-ascii.pbm", "bw-binary.pbm",
           "grey-ascii.pgm", "grey-binary.pgm",
           "rgb-ascii.ppm", "rgb-binary.ppm" ] :
    command += rw_command ("src", f)

# We can't yet write PFM files, so just get the hashes and call it a day
files = [ "test-1.pfm", "test-2.pfm", "test-3.pfm" ]
for f in files:
    command += info_command (imagedir + "/" + f,
                             safematch=True, hash=True)
