#!/usr/bin/env python 

# Copy the grid to both a tiled and scanline version
imagedir = parent + "oiio-images"
command += oiio_app("iconvert") + imagedir + "/grid.tif --scanline scanline.tif > out.txt ;" 
command += oiio_app("iconvert") + imagedir + "/grid.tif --tile 64 64 tiled.tif > out.txt ;" 

# Run the script 
command += "python test_imageoutput.py > out.txt ;"

# compare the outputs
files = [ "grid-image.tif", "grid-scanline.tif", "grid-scanlines.tif",
          "grid-timage.tif", "grid-tile.tif", "grid-tiles.tif" ]
for f in files :
    command += oiio_app("idiff") + f + " " + imagedir + "/grid.tif >> out.txt;"

