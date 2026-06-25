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

command += run_app (pythonbin + " src/make-malformed-channel-pics.py",
                    silent=True)
malformed_channel_files = [
    "inconsistent-bpc-uncompressed.pic",
    "inconsistent-bpc-pure-rle.pic",
    "inconsistent-bpc-mixed-rle.pic",
]
for f in malformed_channel_files:
    command += run_app ("(" + oiio_app("iconvert") + f
                        + " out.null > /dev/null 2>&1 "
                        + "|| echo " + f.replace(".pic", "-rejected") + ")")
