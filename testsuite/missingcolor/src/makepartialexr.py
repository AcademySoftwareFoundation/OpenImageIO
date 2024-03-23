#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import OpenImageIO as oiio
import numpy as np


#######################################################
# Create an exr file with deliberately missing tiles

tilesize = 64
res = 256
tile = np.ones((tilesize,tilesize,3), dtype='float') * 0.75

spec = oiio.ImageSpec(res, res, 3, "half")
spec.tile_width = tilesize
spec.tile_height = tilesize
spec.attribute('openexr:lineOrder', 'randomY')

out = oiio.ImageOutput.create ('partial.exr')
if not out :
    print ('Could not create ImageOutput: ', oiio.geterror())
ok = out.open ('partial.exr', spec)
if not ok :
    print ('Could not open file: ', out.geterror())
count = 0
for y in range(0,res,tilesize) :
    for x in range(0,res,tilesize) :
        # Skip every 7th tile
        if (count % 7) > 1 :
            ok = out.write_tile (x, y, 0, tile)
            if not ok :
                print ('Could not write tile y={} x={}: {}'.format(y, x, out.geterror()))
        count += 1

out.close()

