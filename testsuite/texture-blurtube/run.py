#!/usr/bin/python 

command = testtex_command (parent + "/oiio-images/checker.tx",
                           "--tube --blur 0.1 -d uint8 -o out.tif")
outputs = [ "out.tif" ]
