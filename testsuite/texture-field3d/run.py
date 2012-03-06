#!/usr/bin/python 

command = oiio_app("testtex") + " --nowarp --offset -1 -1 -1 --scalest 2 2 sparse_half.f3d"
outputs = [ "out.exr" ]
