#!/usr/bin/env python

outputs = []

for f in [ "sphere.vdb", "sphereCd.vdb" ]:
    vdbfile = os.path.join('src', f)
    exrfile = os.path.splitext(f)[0] + '.exr'
    command += "%s --nowarp --offset -1 -1 -1 --scalest 2 2 %s -o %s\n" % (oiio_app("testtex"), vdbfile, exrfile)
    outputs.append(exrfile)
