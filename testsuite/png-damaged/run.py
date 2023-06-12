#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

command += rw_command (OIIO_TESTSUITE_IMAGEDIR + "/png/broken",
                       "invalid_gray_alpha_sbit.png",
                       printinfo=False)
