#!/usr/bin/env python

command = testtex_command ("../common/textures/grid.tx",
                           extraargs = "-mipmode 1  -d uint8 -o out.tif")
outputs = [ "out.tif" ]
