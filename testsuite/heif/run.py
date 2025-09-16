#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "IMG_7702_small.heic", "Chimera-AV1-8bit-162.avif", "test-10bit.avif" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

command += oiiotool (os.path.join(imagedir, "test-10bit.avif") +
                     " -d uint10 --cicp \"9,16,9,1\" -o cicp_pq.avif" )
command += info_command ("cicp_pq.avif", safematch=True)

files = [ "greyhounds-looking-for-a-table.heic", "sewing-threads.heic" ]
for f in files:
    command = command + info_command (os.path.join(OIIO_TESTSUITE_IMAGEDIR, f))

# avif conversion is expected to fail if libheif is built without AV1 support
failureok = 1
