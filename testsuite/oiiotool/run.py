#!/usr/bin/python 

# test --create
command += (oiio_app("oiiotool") 
            + " --create 320x240 3 -d uint8 -o black.tif >> out.txt ;\n")
command += oiio_app("oiiotool") + " --stats black.tif >> out.txt ;\n"

# test --pattern constant
command += (oiio_app("oiiotool") 
            + " --pattern constant:color=.1,.2,.3,1 320x240 4"
            + " -o constant.tif >> out.txt ;\n")
command += oiio_app("oiiotool") + " --stats constant.tif >> out.txt ;\n"

# test --fill
command += (oiio_app("oiiotool")
            + " --create 256x256 3 --fill:color=1,.5,.5 256x256"
            + " --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif >> out.txt ;\n")

# test resize
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --resize 256x256 -o resize.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + parent + "/oiio-images/grid.tif"
            + " --resize 25% -o resize2.tif >> out.txt ;\n")

# test fit
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --fit 360x240 -d uint8 -o fit.tif >> out.txt ;\n")

# test --cmul
# First, make a small gray swatch
command += (oiio_app ("oiiotool") + " --pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o cmul-input.exr >> out.txt ;\n")
# Test --cmul val (multiply all channels by the same scalar)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cmul 1.5 -o cmul1.exr >> out.txt ;\n")
# Test --cmul val,val,val... (multiply per-channel scalars)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cmul 1.5,1,0.5 -o cmul2.exr >> out.txt ;\n")

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

# test sequences
command += (oiio_app("oiiotool")
            + "fit.tif -o copyA.1-10#.jpg >> out.txt ;\n");
command += (oiio_app("oiiotool") + " --info copyA.*.jpg >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "filled.tif", "resize.tif", "resize2.tif", "fit.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "chanshuffle.tif", "cmul1.exr", "cmul2.exr",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
