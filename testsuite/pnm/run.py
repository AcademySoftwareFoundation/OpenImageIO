#!/usr/bin/python

imagedir = parent + "/oiio-images/pnm"


#files = [ "oiio-logo-no-alpha.png",  "oiio-logo-with-alpha.png" ]
#for f in files:
#    command += rw_command (imagedir,  f)


# We can't yet write PFM files, so just get the hashes and call it a day
files = [ "test-1.pfm", "test-2.pfm", "test-3.pfm" ]
for f in files:
    command += info_command (imagedir+"/"+f, safematch=True, hash=True)
    #command += rw_command (imagedir,  f)
