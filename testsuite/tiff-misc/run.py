#!/usr/bin/env python

# Miscellaneous TIFF-related tests

outputs = [ ]

# Regression test -- we once had a bug where 'separate' planarconfig
# tiled float files would have data corrupted by a buffer overwrite.
command += oiiotool("--pattern checker 128x128 4 --tile 64 64 --planarconfig separate -d float -o check1.tif")
outputs += [ "check1.tif" ]

# Test bug we had until OIIO 2.3 when reading planarconfig=separate files
# (fixed by #2757) that was not detected by the uncompressed file. So copy
# to force compression in order to properly test:
command += rw_command ("src", "separate.tif")
