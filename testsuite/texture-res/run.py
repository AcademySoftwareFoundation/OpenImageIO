#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

command = (oiio_app("testtex") + " -res 256 256 --nowarp "
           + OIIO_TESTSUITE_IMAGEDIR + "/miplevels.tx"
           + " -o out.tif ;\n")
command += diff_command ("out.tif", "ref/out.tif", "--fail 0.0005 --warn 0.0005")

outputs = [ ]
