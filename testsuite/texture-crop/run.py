#!/usr/bin/env python 

command += oiiotool(parent + "/oiio-images/grid.tif " +
                    "--crop 512x512+200+100 -o grid-crop.tif")
command += maketx_command ("grid-crop.tif",
                           "grid-crop.tx")
command += testtex_command ("grid-crop.tx", "--wrap black")

outputs = [ "out.exr" ]
