#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# These tests are checking the openjph library that can optionally be compiled into the Jpeg2000
# plugin of OIIO. If the library is not enabled, these will fail.

# Capture stderr too, so the rejection messages below are checked.
redirect = ' >> out.txt 2>&1 '

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " -o test.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "test.j2c")

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/dpx/dpx_nuke_10bits_rgb.dpx"
                    " -o testdpx.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/dpx/dpx_nuke_10bits_rgb.dpx", "testdpx.j2c")


command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " --attrib qstep 0.03 -o testcompress.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "testcompress.j2c", extraargs="-fail 0.11")

# Malformed HTJ2K headers, built by src/make_malformed_htj2k.py. The HTJ2K
# reader allocates the whole image up front from the SIZ dimensions, so an
# oversized claim has to be rejected at open time rather than at allocation.
command += oiiotool("--info -v src/bomb-resolution.j2c", failureok = True)
command += oiiotool("--info -v src/bomb-ratio.j2c", failureok = True)
# ... while a small valid codestream still reads.
command += oiiotool("--info -v --hash src/valid-16x16.j2c", failureok = True)
