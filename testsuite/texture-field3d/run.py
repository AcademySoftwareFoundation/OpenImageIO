#!/usr/bin/env python

command = oiio_app("testtex") + " --nowarp --offset -1 -1 -1 --scalest 2 2 src/sparse_half.f3d"
outputs = [ "out.exr" ]
