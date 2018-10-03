#!/usr/bin/env python

import shutil

# Make a copy called "misnamed.exr" that's actually a TIFF file
shutil.copyfile (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif", "misnamed.exr")

# Now see if it is read correctly
command = info_command ("misnamed.exr")
