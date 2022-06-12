#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

files = [ "oiio-logo-no-alpha.png",  "oiio-logo-with-alpha.png" ]
for f in files:
        command += rw_command (OIIO_TESTSUITE_IMAGEDIR,  f)

command += rw_command (OIIO_TESTSUITE_IMAGEDIR + "/png/broken",
                       "invalid_gray_alpha_sbit.png",
                       printinfo=False)
