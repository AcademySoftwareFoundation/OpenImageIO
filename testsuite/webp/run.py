#!/usr/bin/env python

files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
    # We should also test read/write, but it's really hard to because it's
    # a lossy format and is not stable under the round trip
    # command += rw_command (OIIO_TESTSUITE_IMAGEDIR, f,
    #                        extraargs='-attrib compression lossless')
