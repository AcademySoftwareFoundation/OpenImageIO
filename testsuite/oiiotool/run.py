#!/usr/bin/python 

command = (oiio_app ("oiiotool") + " " 
           + parent + "/oiio-images/grid.tif"
           + " --resize 256x256 -o resize.tif >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "resize.tif" ]

#print "Running this command:\n" + command + "\n"
