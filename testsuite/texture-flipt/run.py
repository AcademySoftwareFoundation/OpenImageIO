#!/usr/bin/env python 

command = testtex_command ("../common/textures/grid.tx",
                           "-flipt -res 128 128 -d uint8 -o out.tif")
outputs = [ "out.tif" ]
