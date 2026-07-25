#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

command += rw_command (OIIO_TESTSUITE_IMAGEDIR + "/broken",
                       "invalid_gray_alpha_sbit.png",
                       printinfo=False)

# These carry malformed Exif payloads in a PNG eXIf chunk, reaching the same
# shared decoder as the .jpg fixtures in testsuite/jpeg-corrupt/src. The eXIf
# length field is 31 bits, so this is the route where a deeply nested IFD
# chain can be made large enough to exhaust the stack.
command += info_command ("src/exif-utf8-type.png", safematch=True)
command += info_command ("src/exif-deep-ifds.png", safematch=True)
