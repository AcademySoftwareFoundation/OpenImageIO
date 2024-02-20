#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

filename = (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
command += info_command (filename)
command += oiiotool (filename + " -o ./tahoe-gps.jpg")
command += info_command ("tahoe-gps.jpg", safematch=True)
