#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Miscellaneous TIFF-related tests

# save the error output
redirect = " >> out.txt 2>&1 "

# Regression test -- we once had a bug where 'separate' planarconfig
# tiled float files would have data corrupted by a buffer overwrite.
command += oiiotool("--pattern checker 128x128 4 --tile 64 64 --planarconfig separate -d float -o check1.tif")

# Test bug we had until OIIO 2.3 when reading planarconfig=separate files
# (fixed by #2757) that was not detected by the uncompressed file. So copy
# to force compression in order to properly test:
command += rw_command ("src", "separate.tif")

# Test bugs we had until OIIO 2.4 for these corrupt file
command += oiiotool ("--oiioattrib try_all_readers 0 --info -v src/corrupt1.tif", failureok = True)
command += oiiotool ("--oiioattrib try_all_readers 0 --info -v src/crash-1633.tif", failureok = True)
command += oiiotool ("--oiioattrib try_all_readers 0 --info src/crash-1643.tif -o out.exr", failureok = True)
command += iconvert ("src/crash-1709.tif crash-1709.exr", failureok=True)

outputs = [ "check1.tif", "out.txt" ]
