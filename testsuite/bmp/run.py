#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

files = [ "g01bg.bmp", "g01bw.bmp", "g01p1.bmp", "g01wb.bmp",
          "g04.bmp", "g04p4.bmp", "g04rle.bmp",
          "g08.bmp", "g08os2.bmp", "g08p64.bmp", "g08p256.bmp",
          "g08pi64.bmp", "g08pi256.bmp",
          "g08res11.bmp", "g08res21.bmp", "g08res22.bmp",
          "g08s0.bmp", "g08w124.bmp", "g08w125.bmp", "g08w126.bmp",
          "g08rle.bmp", "g08offs.bmp",
          "g24.bmp", "g32bf.bmp", "g32def.bmp", "g32bf.bmp"
           ]
for f in files :
    command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f)

# TODO: seems broken: g16bf555.bmp,
#                     g16bf565.bmp, g16def555.bmp

# Test BMR version 5
command += rw_command ("src", "g01bg2-v5.bmp")

# Regression test for old OS2 flavor of BMP.
# See https://github.com/OpenImageIO/oiio/issues/2898
command += rw_command ("src", "PRINTER.BMP")

# Test BMP of the 56-byte DIB header variety
command += rw_command ("../oiio-images/bmp", "gracehopper.bmp")

# See if we handle this corrupt file with a useful error message
command += info_command ("src/decodecolormap-corrupt.bmp")
