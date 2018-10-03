#!/usr/bin/env python

filename = (OIIO_TESTSUITE_IMAGEDIR + "/tahoe-gps.jpg")
command += info_command (filename)
command += oiiotool (filename + " -o ./tahoe-gps.jpg")
command += info_command ("tahoe-gps.jpg", safematch=True)
