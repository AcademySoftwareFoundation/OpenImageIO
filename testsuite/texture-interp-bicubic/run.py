#!/usr/bin/env python

command = testtex_command (parent + "/oiio-images/grid.tx",
                           extraargs = "-interpmode 2  -d uint8 -o out.tif")
outputs = [ "out.tif" ]
