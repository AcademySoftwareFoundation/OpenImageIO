#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Make an .hdr image out of an openexr image -- tests write
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/ScanLines/MtTamWest.exr -o MtTamWest.hdr")
# read and print stats, that tests the read
command += oiiotool ("-v -info -stats MtTamWest.hdr")
# To double check, get stats on the original for comparison -- should be close
command += oiiotool ("-stats " + OIIO_TESTSUITE_IMAGEDIR+"/ScanLines/MtTamWest.exr")
