#!/usr/bin/env python 

redirect = " >> stats.txt"
command = testtex_command ("../common/textures/grid.tx -res 64 48 -v --teststatquery --runstats")
outputs = [ "out.exr" ]
