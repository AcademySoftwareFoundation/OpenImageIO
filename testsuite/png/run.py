#!/usr/bin/env python

files = [ "oiio-logo-no-alpha.png",  "oiio-logo-with-alpha.png" ]
for f in files:
        command += rw_command (OIIO_TESTSUITE_IMAGEDIR,  f)
