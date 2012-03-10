#!/usr/bin/python 

command = testtex_command (parent + "/oiio-images/grid.tx", "-res 256 256 -nowarp --width 0 --blur 0.1 --wrap clamp -d uint8 -o out.tif")
outputs = [ "out.tif" ]
