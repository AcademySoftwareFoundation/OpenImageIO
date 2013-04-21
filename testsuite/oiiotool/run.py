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

# test resample
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --resample 128x128 -o resample.tif >> out.txt ;\n")

# test resize
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --resize 256x256 -o resize.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + parent + "/oiio-images/grid.tif"
            + " --resize 25% -o resize2.tif >> out.txt ;\n")

# test extreme resize
command += (oiio_app ("oiiotool")
            + parent + "/oiio-images/grid.tif"
            + " --resize 64x64 -o resize64.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool")
            + "resize64.tif "
            + " --resize 512x512 -o resize512.tif >> out.txt ;\n")

# test fit
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --fit 360x240 -d uint8 -o fit.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --fit 240x360 -d uint8 -o fit2.tif >> out.txt ;\n")

# test --cmul
# First, make a small gray swatch
command += (oiio_app ("oiiotool") + " --pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o cmul-input.exr >> out.txt ;\n")
# Test --cmul val (multiply all channels by the same scalar)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cmul 1.5 -o cmul1.exr >> out.txt ;\n")
# Test --cmul val,val,val... (multiply per-channel scalars)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cmul 1.5,1,0.5 -o cmul2.exr >> out.txt ;\n")

# Test --cadd val (multiply all channels by the same scalar)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cadd 0.25 -o cadd1.exr >> out.txt ;\n")
# Test --cadd val,val,val... (multiply per-channel scalars)
command += (oiio_app ("oiiotool")
            + " cmul-input.exr --cadd 0,0.25,-0.25 -o cadd2.exr >> out.txt ;\n")

# Test --add
command += (oiio_app ("oiiotool")
            + " --pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --add -d half -o add.exr >> out.txt ;\n")
# Test --sub
command += (oiio_app ("oiiotool")
            + " --pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --sub -d half -o sub.exr >> out.txt ;\n")

# test histogram generation
command += (oiio_app ("oiiotool") + " "
            + "ref/histogram_input.png"
            + " --histogram 256x256 0 "
            + "-o histogram_regular.tif >> out.txt ;\n")
command += (oiio_app ("oiiotool") + " "
            + "ref/histogram_input.png"
            + " --histogram:cumulative=1 256x256 0 "
            + "-o histogram_cumulative.tif >> out.txt ;\n")

# test paste
command += (oiio_app("oiiotool")
            + parent + "/oiio-images/grid.tif "
            + "--pattern checker 256x256 3 --paste +150+75 "
            + "-o pasted.tif >> out.txt ;\n")

# test mosaic
command += (oiio_app("oiiotool")
            + "--pattern constant:color=1,0,0 50x50 3 "
            + "--pattern constant:color=0,1,0 50x50 3 "
            + "--pattern constant:color=0,0,1 50x50 3 "
            + "--pattern constant:color=1,1,1 50x50 3 "
            + "--mosaic:pad=10 2x2 "
            + "-d uint8 -o mosaic.tif >> out.txt ;\n")

# test flip
command += (oiio_app ("oiiotool")
            + "image.tif --flip -o flip.tif >> out.txt ;\n")

# test flop
command += (oiio_app ("oiiotool")
            + "image.tif --flop -o flop.tif >> out.txt ;\n")

# test flip-flop
command += (oiio_app ("oiiotool")
            + "image.tif --flipflop -o flipflop.tif >> out.txt ;\n")

# test transpose
command += (oiio_app ("oiiotool")
            + "image.tif --transpose -o transpose.tif >> out.txt ;\n")

# test cshift
command += (oiio_app ("oiiotool")
            + "image.tif --cshift +100+50 -o cshift.tif >> out.txt ;\n")

# test channel shuffling
command += (oiio_app ("oiiotool") + " " 
            + parent + "/oiio-images/grid.tif"
            + " --ch =0.25,B,G -o chanshuffle.tif >> out.txt ;\n")

# test hole filling
command += (oiio_app ("oiiotool") + " " 
            + "ref/hole.tif --fillholes -o tahoe-filled.tif >> out.txt ;\n")

# test clamping
command += (oiio_app ("oiiotool")
            + parent + "/oiio-images/grid.tif --resize 50%"
            + " --clamp:min=0.2:max=,,0.5,1 -o grid-clamped.tif >> out.txt ;\n")

# test unpremult/premult
command += (oiio_app ("oiiotool")
            + " --pattern constant:color=.1,.1,.1,1 100x100 4 " 
            + " --fill:color=.2,.2,.2,.5 30x30+50+50 "
            + " -d half -o premulttarget.exr >> out.txt ;\n")
command += (oiio_app ("oiiotool")
            + " premulttarget.exr --unpremult -o unpremult.exr >> out.txt ;\n")
command += (oiio_app ("oiiotool")
            + " unpremult.exr --premult -o premult.exr >> out.txt ;\n")

# test kernel
command += (oiio_app ("oiiotool")
            + "--kernel bspline 15x15 -o bsplinekernel.exr >> out.txt ;\n")

# test convolve
command += (oiio_app ("oiiotool")
            + "tahoe-small.tif --kernel bspline 15x15 --convolve "
            + "-d uint8 -o bspline-blur.tif >> out.txt ;\n")

# test blur
command += (oiio_app ("oiiotool")
            + "tahoe-small.tif --blur 5x5 "
            + "-d uint8 -o gauss5x5-blur.tif >> out.txt ;\n")

# test unsharp mask
command += (oiio_app ("oiiotool")
            + "tahoe-small.tif --unsharp "
            + "-d uint8 -o unsharp.tif >> out.txt ;\n")

# test fft, ifft
command += (oiio_app ("oiiotool")
            + "tahoe-small.tif --ch 2 --fft -o fft.exr >> out.txt ;\n")
command += (oiio_app ("oiiotool")
            + "fft.exr --ifft --ch 0,0,0 -o ifft.exr >> out.txt ;\n")



# test sequences
command += (oiio_app("oiiotool")
            + "fit.tif -o copyA.1-10#.jpg >> out.txt ;\n");
command += (oiio_app("oiiotool") + " --info copyA.*.jpg >> out.txt ;\n")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "filled.tif", "resample.tif", "resize.tif", "resize2.tif",
            "resize64.tif", "resize512.tif",
            "fit.tif", "fit2.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "pasted.tif", "mosaic.tif",
            "flip.tif", "flop.tif", "flipflop.tif", "transpose.tif",
            "cshift.tif",
            "chanshuffle.tif", "cmul1.exr", "cmul2.exr",
            "cadd1.exr", "cadd2.exr",
            "add.exr", "sub.exr",
            "tahoe-filled.tif",
            "grid-clamped.tif",
            "unpremult.exr", "premult.exr",
            "bsplinekernel.exr", "bspline-blur.tif",
            "gauss5x5-blur.tif", "unsharp.tif",
            "fft.exr", "ifft.exr",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
