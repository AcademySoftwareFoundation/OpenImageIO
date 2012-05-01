#!/usr/bin/python 

# test resize
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --resize 256x256 -o resize.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + parent + "/oiio-images/grid.tif"
            + " --resize 25% -o resize2.tif >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "resize.tif", "resize2.tif", "out.txt" ]

#print "Running this command:\n" + command + "\n"
