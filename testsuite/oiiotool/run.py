#!/usr/bin/python 

# test --create
command += (oiio_app("oiiotool") 
            + " --create 320x240 3 -o black.tif >> out.txt ;\n")
command += oiio_app("oiiotool") + " --stats black.tif >> out.txt ;\n"

# test --pattern constant
command += (oiio_app("oiiotool") 
            + " --pattern constant:color=.1,.2,.3,1 320x240 4"
            + " -o constant.tif >> out.txt ;\n")
command += oiio_app("oiiotool") + " --stats constant.tif >> out.txt ;\n"

# test resize
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --resize 256x256 -o resize.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + parent + "/oiio-images/grid.tif"
            + " --resize 25% -o resize2.tif >> out.txt ;\n")

# test histogram generation
command += (oiio_app ("oiiotool") + " "
            + "ref/histogram_input.png"
            + " --histogram 256x256 0 "
            + "-o histogram_regular.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + "ref/histogram_input.png"
            + " --histogram:cumulative=1 256x256 0 "
            + "-o histogram_cumulative.tif >> out.txt ;\n")

# test channel shuffling
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --ch =0.25,B,G -o chanshuffle.tif >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "resize.tif", "resize2.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "chanshuffle.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
