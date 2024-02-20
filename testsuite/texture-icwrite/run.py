#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# test 1: seed top level, no MIP map
command += testtex_command (OIIO_TESTSUITE_IMAGEDIR + " -res 256 256 -d uint8 -o out1.tif --testicwrite 1 blah")
# test 2: seed top level, automip
command += testtex_command (OIIO_TESTSUITE_IMAGEDIR + " -res 256 256 -d uint8 -o out2.tif --testicwrite 1 --automip blah")

# test 3: procedural MIP map
command += testtex_command (OIIO_TESTSUITE_IMAGEDIR + " -res 256 256 -d uint8 -o out3.tif --testicwrite 2 blah")
# test 4: procedural top level, automip
command += testtex_command (OIIO_TESTSUITE_IMAGEDIR + " -res 256 256 -d uint8 -o out4.tif --testicwrite 2 --automip blah")

outputs = [ "out1.tif", "out2.tif", "out3.tif", "out4.tif" ]
