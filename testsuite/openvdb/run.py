#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

outputs = []

for f in [ "sphere.vdb", "sphereCd.vdb" ]:
    vdbfile = os.path.join('src', f)
    exrfile = os.path.splitext(f)[0] + '.exr'
    command += "%s --nowarp --offset -1 -1 -1 --scalest 2 2 %s -o %s\n" % (oiio_app("testtex"), vdbfile, exrfile)
    outputs.append(exrfile)

command += oiiotool ("-echo \"info openvdb\" --info -v --dumpdata:empty=0 src/sphere.vdb")

# outputs.append("out.txt")
