#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Test the filters

import OpenImageIO as oiio

filters = oiio.get_string_attribute("filter_list").split(";")
print ("all filters: ", filters)
i = 0
outputs = [ ]

delta = oiio.ImageBuf(oiio.ImageSpec(16, 16, 1, "float"))
oiio.ImageBufAlgo.render_point(delta, 8, 8)

for f in filters:
    buf = oiio.ImageBufAlgo.resize(delta, f, roi=oiio.ROI(0, 80, 0, 80, 0, 1))
    # Make a marker different for each filter so they dont compare against
    # each other
    oiio.ImageBufAlgo.render_point(buf, i, 0)
    i += 1
    buf.write("{}.exr".format(f), "half")
    outputs += [ "{}.exr".format(f) ]

outputs += [ "out.txt" ]
