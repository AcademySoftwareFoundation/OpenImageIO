#!/usr/bin/env python

redirect = ' >> out.txt 2>&1 '

command += run_app (oiio_app("igrep") + " -i -E wg ../oiio-images/tahoe-gps.jpg")

