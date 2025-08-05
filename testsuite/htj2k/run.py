#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# These tests are checking the openjph library that can optionally be compiled into the Jpeg2000
# plugin of OIIO. If the library is not enabled, these will fail.

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " -o test.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "test.j2c")

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/dpx_nuke_10bits_rgb.dpx"
                    " -o testdpx.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/dpx_nuke_10bits_rgb.dpx", "testdpx.j2c")


command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " --attrib qstep 0.03 -o testcompress.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "testcompress.j2c", extraargs="-fail 0.11")