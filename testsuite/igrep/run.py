#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ' >> out.txt 2>&1 '

command += run_app (oiio_app("igrep") + " -i -E wg ../oiio-images/tahoe-gps.jpg")

