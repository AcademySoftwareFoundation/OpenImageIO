#!/usr/bin/env python 

# Create some test images we need
command += oiiotool ("--create 320x240 3 -d uint8 -o black.tif")
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o grey128.exr")
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 64x64 3 -d half -o grey64.exr")
command += oiiotool ("--create 256x256 3 --fill:color=1,.5,.5 256x256 --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif")


# test --autotrim
command += oiiotool ("black.tif --fill:color=0,1,0 80x80+100+100 --autotrim -d uint8 -o autotrim.tif")

# test --colorcount  (using the results of the --fill test)
command += oiiotool ("filled.tif --colorcount:eps=.1,.1,.1 0,0,0:1,.5,.5:0,1,0")

# test --rangecheck  (using the results of the --fill test)
command += oiiotool ("filled.tif --rangecheck 0,0,0 1,0.9,1")

# test --rangecompress & --rangeexpand
command += oiiotool ("src/tahoe-small.tif --rangecompress -d uint8 -o rangecompress.tif")
command += oiiotool ("rangecompress.tif --rangeexpand -d uint8 -o rangeexpand.tif")
command += oiiotool ("src/tahoe-small.tif --rangecompress:luma=1 -d uint8 -o rangecompress-luma.tif")
command += oiiotool ("rangecompress-luma.tif --rangeexpand:luma=1 -d uint8 -o rangeexpand-luma.tif")

# test resample
command += oiiotool (parent + "/oiio-images/grid.tif --resample 128x128 -o resample.tif")

# test resize
command += oiiotool (parent + "/oiio-images/grid.tif --resize 256x256 -o resize.tif")
command += oiiotool (parent + "/oiio-images/grid.tif --resize 25% -o resize2.tif")

# test extreme resize
command += oiiotool (parent + "/oiio-images/grid.tif --resize 64x64 -o resize64.tif")
command += oiiotool ("resize64.tif --resize 512x512 -o resize512.tif")

# test fit
command += oiiotool (parent + "/oiio-images/grid.tif --fit 360x240 -d uint8 -o fit.tif")
command += oiiotool (parent + "/oiio-images/grid.tif --fit 240x360 -d uint8 -o fit2.tif")
# regression test: --fit without needing resize used to be problematic
command += oiiotool ("src/tahoe-tiny.tif --fit 128x128 -d uint8 -o fit3.tif")

# test --pixelaspect
command += oiiotool ("src/tahoe-small.tif -resize 256x192 --pixelaspect 2.0 -d uint8 -o pixelaspect.tif")

# test rotate
command += oiiotool ("resize.tif --rotate 45 -o rotated.tif")
command += oiiotool ("resize.tif --rotate:center=50,50 45 -o rotated-offcenter.tif")
command += oiiotool ("resize.tif --rotate 45 --rotate 90 --rotate 90 --rotate 90 --rotate 45 -o rotated360.tif")

# test warp
command += oiiotool ("resize.tif --warp 0.7071068,0.7071068,0,-0.7071068,0.7071068,0,128,-53.01933,1 -o warped.tif")

# Test --add
command += oiiotool ("--pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --add -d half -o add.exr")
# Test --addc val (add to all channels the same scalar)
command += oiiotool ("grey128.exr --addc 0.25 -o cadd1.exr")
# Test --addc val,val,val... (add per-channel scalars)
command += oiiotool ("grey128.exr --addc 0,0.25,-0.25 -o cadd2.exr")

# Test --sub, subc
command += oiiotool ("--pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --sub -d half -o sub.exr")
command += oiiotool ("--pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --subc 0.1,0.1,0.1 -d half -o subc.exr")

# test --mul of images
command += oiiotool ("grey64.exr -pattern constant:color=1.5,1,0.5 64x64 3 --mul -o mul.exr")
# Test --mulc val (multiply all channels by the same scalar)
command += oiiotool ("grey128.exr --mulc 1.5 -o cmul1.exr")
# Test --mulc val,val,val... (multiply per-channel scalars)
command += oiiotool ("grey128.exr --mulc 1.5,1,0.5 -o cmul2.exr")

# Test --divc val (divide all channels by the same scalar)
command += oiiotool ("grey64.exr --divc 2.0 -d half -o divc1.exr")
# Test --divc val,val,val... (divide per-channel scalars)
command += oiiotool ("grey64.exr --divc 2.0,1,0.5 -d half -o divc2.exr")
# Test --div of images
command += oiiotool ("grey64.exr --pattern constant:color=2.0,1,0.5 64x64 3 "
                   + "--div -d half -o div.exr")

# test --mad of images
command += oiiotool ("grey64.exr -pattern constant:color=1.5,1,0.5 64x64 3 "
                   + "-pattern constant:color=.1,.1,.1 64x64 3 --mad -o mad.exr")

# test --invert
command += oiiotool ("src/tahoe-small.tif --invert -o invert.tif")

# Test --powc val (raise all channels by the same power)
command += oiiotool ("grey128.exr --powc 2 -o cpow1.exr")
# Test --powc val,val,val... (per-channel powers)
command += oiiotool ("grey128.exr --powc 2,2,1 -o cpow2.exr")

# Test --abs, --absdiff, --absdiffc
# First, make a test image that's 0.5 on the left, -0.5 on the right
command += oiiotool ("-pattern constant:color=-0.25,-0.25,-0.25 64x128 3 "
                   + "-pattern constant:color=0.5,0.5,0.5 64x128 3 "
                   + "-mosaic 2x1 -d half -o negpos.exr")
command += oiiotool ("negpos.exr -abs -o abs.exr")
command += oiiotool ("negpos.exr -pattern constant:color=0.2,0.2,0.2 128x128 3 "
                   + "-absdiff -d half -o absdiff.exr")
command += oiiotool ("negpos.exr -absdiffc 0.2,0.2,0.2 -d half -o absdiffc.exr")

# test --chsum
command += oiiotool ("src/tahoe-small.tif --chsum:weight=.2126,.7152,.0722 "
            + "-d uint8 -o chsum.tif")

# test histogram generation
command += oiiotool ("ref/histogram_input.png --histogram 256x256 0 "
            + "-o histogram_regular.tif")
command += oiiotool ("ref/histogram_input.png --histogram:cumulative=1 256x256 0 "
            + "-o histogram_cumulative.tif")

# test --crop
command += oiiotool (parent + "/oiio-images/grid.tif --crop 100x400+50+200 -o crop.tif")

# test --cut
command += oiiotool (parent + "/oiio-images/grid.tif --cut 100x400+50+200 -o cut.tif")

# test paste
command += oiiotool (parent + "/oiio-images/grid.tif "
            + "--pattern checker 256x256 3 --paste +150+75 -o pasted.tif")

# test --trim
command += oiiotool ("--create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 "
                     + " -rotate 30 -trim -origin +0+0 -fullpixels -d uint8 -o trim.tif")

# test --trim, tricky case of multiple subimages
command += oiiotool (  "-a --create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 -rotate 30 "
                     + "--create 320x240 3 -fill:color=.5,.5,.1 100x10+70+70 -rotate 140 "
                     + "--siappend -trim -origin +0+0 -fullpixels -d uint8 -o trimsubimages.tif")

# test mosaic
command += oiiotool ("--pattern constant:color=1,0,0 50x50 3 "
            + "--pattern constant:color=0,1,0 50x50 3 "
            + "--pattern constant:color=0,0,1 50x50 3 "
            + "--pattern constant:color=1,1,1 50x50 3 "
            + "--mosaic:pad=10 2x2 -d uint8 -o mosaic.tif")

# test flip
command += oiiotool ("src/image.tif --flip -o flip.tif")
command += oiiotool ("src/image.tif --crop 180x140+30+30 --flip -o flip-crop.tif")

# test flop
command += oiiotool ("src/image.tif --flop -o flop.tif")
command += oiiotool ("src/image.tif --crop 180x140+30+30 --flop -o flop-crop.tif")

# test rotate90
command += oiiotool ("src/image.tif --rotate90 -o rotate90.tif")
command += oiiotool ("src/image.tif --crop 180x140+30+30 --rotate90 -o rotate90-crop.tif")

# test rotate270
command += oiiotool ("src/image.tif --rotate270 -o rotate270.tif")
command += oiiotool ("src/image.tif --crop 180x140+30+30 --rotate270 -o rotate270-crop.tif")

# test rotate180
command += oiiotool ("src/image.tif --rotate180 -o flipflop.tif")
command += oiiotool ("src/image.tif --crop 160x120+30+30 --rotate180 -o flipflop-crop.tif")

# Tricky: make image, rotate, set Orientation, and then re-orient.
# Make it half size so it can't accidentally match to another test image
# for the rotation tests.
command += oiiotool ("src/image.tif --resample 160x120 --rotate90  --orientccw --reorient -o reorient1.tif")
command += oiiotool ("src/image.tif --resample 160x120 --rotate180 --orient180 --reorient -o reorient2.tif")
command += oiiotool ("src/image.tif --resample 160x120 --rotate270 --orientcw  --reorient -o reorient3.tif")

# test transpose
command += oiiotool ("src/image.tif --transpose -o transpose.tif")
command += oiiotool ("src/image.tif --crop 160x120+30+30 --transpose -o transpose-crop.tif")

# test cshift
command += oiiotool ("src/image.tif --cshift +100+50 -o cshift.tif")

# test channel shuffling
command += oiiotool (parent + "/oiio-images/grid.tif"
            + " --ch =0.25,B,G -o chanshuffle.tif")

# test --ch to separate RGBA from an RGBAZ file
command += oiiotool ("src/rgbaz.exr --ch R,G,B,A -o ch-rgba.exr")
command += oiiotool ("src/rgbaz.exr --ch Z -o ch-z.exr")

# test --chappend to merge RGBA and Z
command += oiiotool ("ch-rgba.exr ch-z.exr --chappend -o chappend-rgbaz.exr")

# test --chnames to rename channels
command += oiiotool ("src/rgbaz.exr --chnames Red,,,,Depth -o chname.exr")
command += info_command ("chname.exr", safematch=1)

# test -d to change data formats
command += oiiotool ("src/rgbaz.exr -d half -o allhalf.exr")
command += info_command ("allhalf.exr", safematch=1)

# test -d NAME=fmt to change data format of one channel, and to make
# sure oiiotool will output per-channel formats.
command += oiiotool ("src/rgbaz.exr -d half -d Z=float -o rgbahalf-zfloat.exr")
command += info_command ("rgbahalf-zfloat.exr", safematch=1)

# test hole filling
command += oiiotool ("ref/hole.tif --fillholes -o tahoe-filled.tif")

# test clamping
command += oiiotool (parent + "/oiio-images/grid.tif --resize 50%"
            + " --clamp:min=0.2:max=,,0.5,1 -o grid-clamped.tif")

# test unpremult/premult
command += oiiotool ("--pattern constant:color=.1,.1,.1,1 100x100 4 " 
            + " --fill:color=.2,.2,.2,.5 30x30+50+50 "
            + " -d half -o premulttarget.exr")
command += oiiotool ("premulttarget.exr --unpremult -o unpremult.exr")
command += oiiotool ("unpremult.exr --premult -o premult.exr")

# test kernel
command += oiiotool ("--kernel bspline 15x15 -o bsplinekernel.exr")

# test convolve
command += oiiotool ("src/tahoe-small.tif --kernel bspline 15x15 --convolve "
            + "-d uint8 -o bspline-blur.tif")

# test blur
command += oiiotool ("src/tahoe-small.tif --blur 5x5 -d uint8 -o gauss5x5-blur.tif")

# test median filter
command += oiiotool ("src/tahoe-small.tif --median 5x5 -d uint8 -o tahoe-median.tif")

# test unsharp mask
command += oiiotool ("src/tahoe-small.tif --unsharp -d uint8 -o unsharp.tif")

# test unsharp mask with median filter
command += oiiotool ("src/tahoe-small.tif --unsharp:kernel=median -d uint8 -o unsharp-median.tif")

# test laplacian
command += oiiotool ("src/tahoe-tiny.tif --laplacian -d uint8 -o tahoe-laplacian.tif")

# test fft, ifft
command += oiiotool ("src/tahoe-tiny.tif --ch 2 --fft -d float -o fft.exr")
command += oiiotool ("fft.exr --ifft --ch 0 -d float -o ifft.exr")

# test --polar, --unpolar
# note that fft.exr that we built above is in complex form
command += oiiotool ("fft.exr --polar -d float -o polar.exr")
command += oiiotool ("polar.exr --unpolar -d float -o unpolar.exr")

# test labels
command += oiiotool (
            " --pattern constant:color=0.5,0.0,0.0 128x128 3 --label R " +
            " --pattern constant:color=0.0,0.5,0.0 128x128 3 --label G " +
            " --pattern constant:color=0.5,0.0,0.0 128x128 3 --label B " +
            " --pop --pop --pop " +
            " R G --add -d half -o labeladd.exr")


# test subimages
command += oiiotool ("--pattern constant:color=0.5,0.0,0.0 64x64 3 " +
                     "--pattern constant:color=0.0,0.5,0.0 64x64 3 " +
                     "--siappend -d half -o subimages-2.exr")
command += oiiotool ("--pattern constant:color=0.5,0.0,0.0 64x64 3 --text A -attrib oiio:subimagename layerA " +
                     "--pattern constant:color=0.0,0.5,0.0 64x64 3 --text B -attrib oiio:subimagename layerB " +
                     "--pattern constant:color=0.0,0.0,0.5 64x64 3 --text C -attrib oiio:subimagename layerC " +
                     "--pattern constant:color=0.5,0.5,0.0 64x64 3 --text D -attrib oiio:subimagename layerD " +
                     "--siappendall -d half -o subimages-4.exr")
command += oiiotool ("subimages-4.exr --subimage 3 -o subimageD3.exr")
command += oiiotool ("subimages-4.exr --subimage layerB -o subimageB1.exr")
command += oiiotool ("subimages-2.exr --sisplit -o subimage2.exr " +
                     "--pop -o subimage1.exr")

# test sequences
command += oiiotool ("fit.tif -o copyA.1-10#.jpg");
command += oiiotool (" --info  " +  " ".join(["copyA.{0:04}.jpg".format(x) for x in range(1,11)]))

# test expression substitution
command += oiiotool ("src/tahoe-small.tif --pattern fill:top=0,0,0,0:bottom=0,0,1,1 " +
                     "{TOP.geom} {TOP.nchannels} -d uint8 -o exprgradient.tif")
command += oiiotool ("src/tahoe-small.tif -cut '{TOP.width-20* 2}x{TOP.height-40+(4*2- 2 ) /6-1}+{TOP.x+100.5-80.5 }+{TOP.y+20}' -d uint8 -o exprcropped.tif")
command += oiiotool ("src/tahoe-small.tif -o exprstrcat{TOP.compression}.tif")

# test --no-autopremult on a TGA file thet needs it.
command += oiiotool ("--no-autopremult src/rgba.tga --ch R,G,B -o rgbfromtga.png")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ 
            "filled.tif",
            "autotrim.tif",
            "resample.tif", "resize.tif", "resize2.tif",
            "resize64.tif", "resize512.tif",
            "fit.tif", "fit2.tif", "fit3.tif",
            "pixelaspect.tif",
            "warped.tif",
            "rotated.tif", "rotated-offcenter.tif", "rotated360.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "crop.tif", "cut.tif", "pasted.tif", "mosaic.tif",
            "trim.tif", "trimsubimages.tif",
            "flip.tif", "flip-crop.tif",
            "flop.tif", "flop-crop.tif",
            "flipflop.tif", "flipflop-crop.tif",
            "rotate90.tif", "rotate90-crop.tif",
            "rotate270.tif", "rotate270-crop.tif",
            "reorient1.tif", "reorient2.tif", "reorient3.tif",
            "transpose.tif", "transpose-crop.tif",
            "cshift.tif",
            "chanshuffle.tif", "ch-rgba.exr", "ch-z.exr",
            "chappend-rgbaz.exr", "chname.exr",
            "add.exr", "cadd1.exr", "cadd2.exr",
            "sub.exr", "subc.exr",
            "mul.exr", "cmul1.exr", "cmul2.exr",
            "div.exr", "divc1.exr", "divc2.exr",
            "mad.exr", "invert.tif",
            "cpow1.exr", "cpow2.exr",
            "abs.exr", "absdiff.exr", "absdiffc.exr",
            "chsum.tif",
            "rgbahalf-zfloat.exr",
            "tahoe-filled.tif",
            "rangecompress.tif", "rangeexpand.tif",
            "rangecompress-luma.tif", "rangeexpand-luma.tif",
            "grid-clamped.tif",
            "unpremult.exr", "premult.exr",
            "bsplinekernel.exr", "bspline-blur.tif",
            "gauss5x5-blur.tif", "tahoe-median.tif",
            "unsharp.tif", "unsharp-median.tif", "tahoe-laplacian.tif",
            "fft.exr", "ifft.exr",
            "polar.exr", "unpolar.exr",
            "labeladd.exr",
            "exprgradient.tif", "exprcropped.tif", "exprstrcatlzw.tif",
            "rgbfromtga.png",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
