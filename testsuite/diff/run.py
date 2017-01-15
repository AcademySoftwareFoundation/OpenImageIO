#!/usr/bin/env python

# Make two images that differ by a particular known pixel value
command += oiiotool("-pattern fill:color=0.1,0.1,0.1 10x10 3 -d float -o img1.exr")
command += oiiotool("-pattern fill:color=0.1,0.1,0.1 10x10 3 -d float -box:color=0.1,0.6,0.1 5,7,5,7 -o img2.exr")

# Now make sure idiff and oiiotool --diff print the right info
failureok = True
command += oiio_app("idiff") + " img1.exr img2.exr >> out.txt ;\n"
command += oiio_app("oiiotool") + " -diff img1.exr img2.exr >> out.txt ;\n"


# Outputs to check against references
outputs = [ "out.txt" ]
