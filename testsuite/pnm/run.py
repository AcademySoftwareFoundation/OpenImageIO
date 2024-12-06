#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

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

# Damaged files
files = [ "src/bad-4552.pgm", "src/bad-4553.pgm" ]
for f in files:
    command += info_command (f, extraargs="--oiioattrib try_all_readers 0 --printstats", failureok=True)
