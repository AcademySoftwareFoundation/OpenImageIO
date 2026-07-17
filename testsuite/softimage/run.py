#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '
 
files = [ "A4.pic", "astone64.pic" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f, extraargs="--stats")

# Regression testing of error handling and corrupt files
command += info_command ("--stats src/broken01.pic",
                         info_program="iinfo", failureok=True)
# A file whose channel packets use different bit depths (here a 16-bit R
# packet and an 8-bit G packet): narrower channels get promoted to the
# widest depth present. This used to overrun the scanline buffer because a
# single bit depth was assumed for all channels.
command += info_command ("--stats src/mixed-bitdepth.pic",
                         info_program="iinfo", failureok=True)
