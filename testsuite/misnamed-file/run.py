#!/usr/bin/python 

import shutil

# Make a copy called "misnamed.exr" that's actually a TIFF file
shutil.copyfile (parent+"/oiio-images/grid.tif", "misnamed.exr")

# Now see if it is read correctly
command = info_command ("misnamed.exr")
