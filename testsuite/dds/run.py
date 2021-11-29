#!/usr/bin/env python

files = [ "sample-DXT1.dds" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
