#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

files = [ "g01bg.bmp", "g04.bmp", "g08.bmp",
          "g16bf555.bmp", "g24.bmp", "g32bf.bmp" ]
for f in files :
    command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f)

# Test BMR version 5
command += rw_command ("src", "g01bg2-v5.bmp")

# Regression test for old OS2 flavor of BMP.
# See https://github.com/OpenImageIO/oiio/issues/2898
command += rw_command ("src", "PRINTER.BMP")

# See if we handle this corrupt file with a useful error message
command += info_command ("src/decodecolormap-corrupt.bmp")
