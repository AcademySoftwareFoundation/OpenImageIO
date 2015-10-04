#!/usr/bin/env python 

# Allow a bit of LSB slop
failthresh = 0.004
failpercent = 0.2

# test --create
command += oiiotool ("--create 320x240 3 -d uint8 -o black.tif")
command += oiiotool ("--stats black.tif")

# test --pattern constant
command += oiiotool ("--pattern constant:color=.1,.2,.3,1 320x240 4 -o constant.tif")
command += oiiotool ("--stats constant.tif")

# test --fill
command += oiiotool ("--create 256x256 3 --fill:color=1,.5,.5 256x256 --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif")

# test --autotrim
command += oiiotool ("black.tif --fill:color=0,1,0 80x80+100+100 --autotrim -d uint8 -o autotrim.tif")

# test --colorcount  (using the results of the --fill test)
command += oiiotool ("filled.tif --colorcount:eps=.1,.1,.1 0,0,0:1,.5,.5:0,1,0")

# test --rangecheck  (using the results of the --fill test)
command += oiiotool ("filled.tif --rangecheck 0,0,0 1,0.9,1")

# test --rangecompress & --rangeexpand
command += oiiotool ("tahoe-small.tif --rangecompress -d uint8 -o rangecompress.tif")
command += oiiotool ("rangecompress.tif --rangeexpand -d uint8 -o rangeexpand.tif")
command += oiiotool ("tahoe-small.tif --rangecompress:luma=1 -d uint8 -o rangecompress-luma.tif")
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
command += oiiotool ("tahoe-tiny.tif --fit 128x128 -d uint8 -o fit3.tif")

# test rotate
command += oiiotool ("resize.tif --rotate 45 -o rotated.tif")
command += oiiotool ("resize.tif --rotate:center=50,50 45 -o rotated-offcenter.tif")
command += oiiotool ("resize.tif --rotate 45 --rotate 90 --rotate 90 --rotate 90 --rotate 45 -o rotated360.tif")

# test warp
command += oiiotool ("resize.tif --warp 0.7071068,0.7071068,0,-0.7071068,0.7071068,0,128,-53.01933,1 -o warped.tif")

# test --cmul
# First, make a small gray swatch
command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o cmul-input.exr")
# Test --cmul val (multiply all channels by the same scalar)
command += oiiotool ("cmul-input.exr --cmul 1.5 -o cmul1.exr")
# Test --cmul val,val,val... (multiply per-channel scalars)
command += oiiotool ("cmul-input.exr --cmul 1.5,1,0.5 -o cmul2.exr")

# Test --cadd val (multiply all channels by the same scalar)
command += oiiotool ("cmul-input.exr --cadd 0.25 -o cadd1.exr")
# Test --cadd val,val,val... (multiply per-channel scalars)
command += oiiotool ("cmul-input.exr --cadd 0,0.25,-0.25 -o cadd2.exr")

# Test --cpow val (raise all channels by the same power)
command += oiiotool ("cmul-input.exr --cpow 2 -o cpow1.exr")
# Test --cpow val,val,val... (per-channel powers)
command += oiiotool ("cmul-input.exr --cpow 2,2,1 -o cpow2.exr")

# Test --add
command += oiiotool ("--pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --add -d half -o add.exr")
# Test --sub
command += oiiotool ("--pattern constant:color=.1,.2,.3 64x64+0+0 3 "
            + " --pattern constant:color=.1,.1,.1 64x64+20+20 3 "
            + " --sub -d half -o sub.exr")

# test --chsum
command += oiiotool ("tahoe-small.tif --chsum:weight=.2126,.7152,.0722 "
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

# test mosaic
command += oiiotool ("--pattern constant:color=1,0,0 50x50 3 "
            + "--pattern constant:color=0,1,0 50x50 3 "
            + "--pattern constant:color=0,0,1 50x50 3 "
            + "--pattern constant:color=1,1,1 50x50 3 "
            + "--mosaic:pad=10 2x2 -d uint8 -o mosaic.tif")

# test flip
command += oiiotool ("image.tif --flip -o flip.tif")
command += oiiotool ("image.tif --crop 180x140+30+30 --flip -o flip-crop.tif")

# test flop
command += oiiotool ("image.tif --flop -o flop.tif")
command += oiiotool ("image.tif --crop 180x140+30+30 --flop -o flop-crop.tif")

# test rotate90
command += oiiotool ("image.tif --rotate90 -o rotate90.tif")
command += oiiotool ("image.tif --crop 180x140+30+30 --rotate90 -o rotate90-crop.tif")

# test rotate270
command += oiiotool ("image.tif --rotate270 -o rotate270.tif")
command += oiiotool ("image.tif --crop 180x140+30+30 --rotate270 -o rotate270-crop.tif")

# test rotate180
command += oiiotool ("image.tif --rotate180 -o flipflop.tif")
command += oiiotool ("image.tif --crop 160x120+30+30 --rotate180 -o flipflop-crop.tif")

# Tricky: make image, rotate, set Orientation, and then re-orient.
# Make it half size so it can't accidentally match to another test image
# for the rotation tests.
command += oiiotool ("image.tif --resample 160x120 --rotate90  --orientccw --reorient -o reorient1.tif")
command += oiiotool ("image.tif --resample 160x120 --rotate180 --orient180 --reorient -o reorient2.tif")
command += oiiotool ("image.tif --resample 160x120 --rotate270 --orientcw  --reorient -o reorient3.tif")

# test transpose
command += oiiotool ("image.tif --transpose -o transpose.tif")
command += oiiotool ("image.tif --crop 160x120+30+30 --transpose -o transpose-crop.tif")

# test cshift
command += oiiotool ("image.tif --cshift +100+50 -o cshift.tif")

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
command += oiiotool ("tahoe-small.tif --kernel bspline 15x15 --convolve "
            + "-d uint8 -o bspline-blur.tif")

# test blur
command += oiiotool ("tahoe-small.tif --blur 5x5 -d uint8 -o gauss5x5-blur.tif")

# test median filter
command += oiiotool ("tahoe-small.tif --median 5x5 -d uint8 -o tahoe-median.tif")

# test unsharp mask
command += oiiotool ("tahoe-small.tif --unsharp -d uint8 -o unsharp.tif")

# test unsharp mask with median filter
command += oiiotool ("tahoe-small.tif --unsharp:kernel=median -d uint8 -o unsharp-median.tif")

# test fft, ifft
command += oiiotool ("tahoe-tiny.tif --ch 2 --fft -d float -o fft.exr")
command += oiiotool ("fft.exr --ifft --ch 0,0,0 -d float -o ifft.exr")

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


# test sequences
command += oiiotool ("fit.tif -o copyA.1-10#.jpg");
command += oiiotool (" --info  " +  " ".join(["copyA.{0:04}.jpg".format(x) for x in range(1,11)]))

# test --no-autopremult on a TGA file thet needs it.
command += oiiotool ("--no-autopremult src/rgba.tga --ch R,G,B -o rgbfromtga.png")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "filled.tif", "autotrim.tif",
            "resample.tif", "resize.tif", "resize2.tif",
            "resize64.tif", "resize512.tif",
            "fit.tif", "fit2.tif", "fit3.tif",
            "warped.tif",
            "rotated.tif", "rotated-offcenter.tif", "rotated360.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "crop.tif", "cut.tif", "pasted.tif", "mosaic.tif",
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
            "cmul1.exr", "cmul2.exr",
            "cadd1.exr", "cadd2.exr",
            "cpow1.exr", "cpow2.exr",
            "add.exr", "sub.exr", "chsum.tif",
            "rgbahalf-zfloat.exr",
            "tahoe-filled.tif",
            "rangecompress.tif", "rangeexpand.tif",
            "rangecompress-luma.tif", "rangeexpand-luma.tif",
            "grid-clamped.tif",
            "unpremult.exr", "premult.exr",
            "bsplinekernel.exr", "bspline-blur.tif",
            "gauss5x5-blur.tif", "tahoe-median.tif",
            "unsharp.tif", "unsharp-median.tif",
            "fft.exr", "ifft.exr",
            "polar.exr", "unpolar.exr",
            "labeladd.exr",
            "rgbfromtga.png",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
