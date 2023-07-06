#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/OpenImageIO/oiio

filename = (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
command += info_command (filename)
command += oiiotool (filename + " -o ./tahoe-gps.jpg")
command += info_command ("tahoe-gps.jpg", safematch=True)
