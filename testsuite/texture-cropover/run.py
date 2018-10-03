#!/usr/bin/env python 

# Tests input image which is partial crop, partial overscan!

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR + "/grid.tif " +
                    "--crop 500x1000+250+0 --fullsize 1000x800+0+100 -o grid-cropover.exr")
command += maketx_command ("grid-cropover.exr",
                           "grid-cropover.tx.exr")
command += testtex_command ("grid-cropover.tx.exr", "--wrap black")

outputs = [ "out.exr" ]
