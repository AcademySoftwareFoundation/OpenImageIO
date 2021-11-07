#!/usr/bin/env python

redirect = " >> out.txt 2>> out.err.txt"

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
command += oiiotool(OIIO_TESTSUITE_IMAGEDIR + "/rla/crash1.rla -o crash1.exr", failureok = True)
command += oiiotool(OIIO_TESTSUITE_IMAGEDIR + "/rla/crash2.rla -o crash2.exr", failureok = True)

outputs = [ "rlacrop.rla", 'out.txt', 'out.err.txt' ]
