#!/usr/bin/env python

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/pnm"

# We can't yet write PFM files, so just get the hashes and call it a day
files = [ "test-1.pfm", "test-2.pfm", "test-3.pfm" ]
for f in files:
    command += info_command (imagedir + "/" + f,
                             safematch=True, hash=True)
