#!/usr/bin/env python

command = testtex_command ("../common/textures/grid.tx",
                           extraargs = "-interpmode 0  -d uint8 -o out.tif")
outputs = [ "out.tif" ]
