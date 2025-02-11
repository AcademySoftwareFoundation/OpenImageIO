#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1"

files = [ "ginsu_a_nc10.rla", "ginsu_a_ncf.rla", "ginsu_rgba_nc8.rla",
          "ginsu_rgb_nc16.rla", "imgmake_rgba_nc10.rla", "ginsu_a_nc16.rla",
          "ginsu_rgba_nc10.rla", "ginsu_rgba_ncf.rla", "ginsu_rgb_nc8.rla",
          "imgmake_rgba_nc16.rla", "ginsu_a_nc8.rla", "ginsu_rgba_nc16.rla",
          "ginsu_rgb_nc10.rla", "ginsu_rgb_ncf.rla", "imgmake_rgba_nc8.rla" ]
for f in files:
    command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f)

# Regression test to ensure crops work
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR +
                     "/ginsu_rgb_nc8.rla -crop 100x100+100+100 -o rlacrop.rla")
# Test corrupted files
command += oiiotool(OIIO_TESTSUITE_IMAGEDIR + "/crash1.rla -o crash1.exr", failureok = True)
command += oiiotool(OIIO_TESTSUITE_IMAGEDIR + "/crash2.rla -o crash2.exr", failureok = True)
command += oiiotool("src/crash-1629.rla -o crash3.exr", failureok = True)
command += oiiotool("src/crash-3951.rla -o crash4.exr", failureok = True)
command += oiiotool("src/crash-1.rla -o crash5.exr", failureok = True)

outputs = [ "rlacrop.rla", 'out.txt' ]
