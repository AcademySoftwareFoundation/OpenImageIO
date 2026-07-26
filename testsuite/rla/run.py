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
command += oiiotool("src/crash-5152.rla -o crash6.exr", failureok = True)
command += oiiotool("src/crash-5159.rla -o crash7.exr", failureok = True)
command += oiiotool("src/crash-badrle.rla -o crash8.exr", failureok = True)

# Malformed inputs built by src/make_malformed_rla.py.
# A 1.2 KB file claiming a 6.8 GB image must be rejected before anything is
# sized from the spec.
command += oiiotool("--info -a -v src/bomb.rla", failureok = True)
# A subimage whose NextOffset points at itself must not present an endless
# supply of subimages; enumeration has to terminate.
command += oiiotool("--info -a -v --hash src/subimage-loop.rla", failureok = True)
# Field-rendered images halve the height, so the scanline offset table has
# more entries than there are scanlines. Reading must stay in bounds.
command += oiiotool("--info -v --hash src/field-rendered.rla", failureok = True)

outputs = [ "rlacrop.rla", 'out.txt' ]
