#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

imagedir = "ref/"
files = [ "norle-8.sgi", "rle-8.sgi", "norle-16.sgi", "rle-16.sgi" ]
for f in files:
    command = command + rw_command (imagedir, f)

# Regression testing of error handling and corrupt files
command += info_command ("--stats src/broken01.sgi",
                         info_program="iinfo", failureok=True)
