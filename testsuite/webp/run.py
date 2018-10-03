#!/usr/bin/env python

files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
for f in files:
    command = command + info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
