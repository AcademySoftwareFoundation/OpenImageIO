#!/usr/bin/python 

# Miscellaneous TIFF-related tests



# Regression test -- we once had a bug where 'separate' planarconfig
# tiled float files would have data corrupted by a buffer overwrite.
command += (oiio_app("oiiotool") + "--pattern checker 128x128 4 --tile 64 64 --planarconfig separate -d float -o check1.tif")

outputs = [ "check1.tif" ]
