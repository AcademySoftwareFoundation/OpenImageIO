#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "IMG_7702_small.heic", "Chimera-AV1-8bit-162.avif" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

files = [ "greyhounds-looking-for-a-table.heic", "sewing-threads.heic" ]
for f in files:
    command = command + info_command (os.path.join(OIIO_TESTSUITE_IMAGEDIR, f))

# avif conversion is expected to fail if libheif is built without AV1 support
failureok = 1
