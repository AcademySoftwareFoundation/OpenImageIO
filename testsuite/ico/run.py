#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

failureok = 1
redirect = ' >> out.txt 2>&1 '

command += rw_command (OIIO_TESTSUITE_IMAGEDIR, "oiio.ico")
command += run_app (oiio_app("iconvert") + " src/bad1.ico out.tif")

