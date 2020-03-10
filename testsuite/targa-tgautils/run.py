#!/usr/bin/env python

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/targa"

files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA" ]
for f in files:
    command += rw_command (imagedir, f)
