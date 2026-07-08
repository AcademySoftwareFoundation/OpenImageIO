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
# broken02.sgi has a corrupt header (zsize=0) whose RLE offset table was sized
# inconsistently with the scanline indexing, causing a null-pointer crash.
command += info_command ("--stats src/broken02.sgi",
                         info_program="iinfo", failureok=True)
# broken03.sgi and broken04.sgi have RLE scanline length/offset table entries
# that are bogus huge values. Truncating them to `int` produced negative
# lengths that turned into huge allocation requests, aborting the process.
command += info_command ("--stats src/broken03.sgi",
                         info_program="iinfo", failureok=True)
command += info_command ("--stats src/broken04.sgi",
                         info_program="iinfo", failureok=True)
