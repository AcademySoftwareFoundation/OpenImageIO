#!/usr/bin/env python 

import OpenImageIO as oiio


# Create a 256x256 image with a red and green ramp.  Without dither, it
# should look very blocky.

try:
    res = 256
    spec = oiio.ImageSpec (res, res, 3, oiio.FLOAT)
    spec.attribute ("oiio:dither", 1)
    b = oiio.ImageBuf (spec)
    for y in range(0,res) :
        for x in range(0,res) :
            val = float(x)/4096.0
            b.setpixel (x, y, (val, val, val))
    b.set_write_format (oiio.UINT8)
    b.write ("ramp.tif")
    print "Done."

except Exception as detail:
    print "Unknown exception:", detail

