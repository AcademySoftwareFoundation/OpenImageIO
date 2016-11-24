#!/usr/bin/env python

import OpenImageIO as oiio

# Make two images that differ by a particular known pixel value
buf = oiio.ImageBuf (oiio.ImageSpec (10, 10, 3, oiio.FLOAT))
oiio.ImageBufAlgo.fill (buf, (0.1,0.1,0.1))
buf.write ("img1.exr")
buf.setpixel (5, 7, (0.1,0.6,0.1))
buf.write ("img2.exr")

# Now make sure idiff and oiiotool --diff print the right info
failureok = True
command += oiio_app("idiff") + " img1.exr img2.exr >> out.txt ;\n"
command += oiio_app("oiiotool") + " -diff img1.exr img2.exr >> out.txt ;\n"


# Outputs to check against references
outputs = [ "out.txt" ]
