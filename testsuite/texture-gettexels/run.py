#!/usr/bin/env python

command = testtex_command ("--gettexels -res 128 128 -d uint8 ../common/textures/grid.tx -o postage.tif")
outputs = [ "postage.tif" ]
