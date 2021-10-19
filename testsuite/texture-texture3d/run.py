#!/usr/bin/env python

command = testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 ../openvdb/src/sphere.vdb")
outputs = [ "out.exr" ]
