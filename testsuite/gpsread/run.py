#!/usr/bin/env python

filename = (parent + "/oiio-images/tahoe-gps.jpg")
command += info_command (filename)
command += oiiotool (filename + " -o ./tahoe-gps.jpg")
command += info_command ("tahoe-gps.jpg", safematch=True)
