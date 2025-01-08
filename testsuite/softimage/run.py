#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

files = [ "A4.pic", "astone64.pic" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f, extraargs="--stats")

