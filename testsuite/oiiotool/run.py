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

# test --trim
command += oiiotool ("--create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 "
                     + " -rotate 30 -trim -origin +0+0 -fullpixels -d uint8 -o trim.tif")

# test --trim, tricky case of multiple subimages
command += oiiotool (  "-a --create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 -rotate 30 "
                     + "--create 320x240 3 -fill:color=.5,.5,.1 100x10+70+70 -rotate 140 "
                     + "--siappend -trim -origin +0+0 -fullpixels -d uint8 -o trimsubimages.tif")

# test channel shuffling
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif"
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
# test hole filling for a cropped image
command += oiiotool ("-pattern checker 64x64+32+32 3 -ch R,G,B,A=1.0 -fullsize 128x128+0+0 --croptofull -fillholes -d uint8 -o growholes.tif")

# Test --min/--max
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--pattern fill:left=0,0,0:right=1,1,1 64x64 3 "
                   + "--min -d uint8 -o min.exr")
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--pattern fill:left=0,0,0:right=1,1,1 64x64 3 "
                   + "--max -d uint8 -o max.exr")
# Test --minc/maxc val (min to all channels the same scalar)
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--minc 0.25 -o cmin1.exr")
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--maxc 0.75 -o cmax1.exr")
# Test --minc/maxc val,val,val... (min per-channel scalars)
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--minc 0.75,0.5,0.25 -o cmin2.exr")
command += oiiotool ("--pattern fill:top=0,0,0:bottom=1,1,1 64x64 3 "
                   + "--maxc 0.75,0.5,0.25 -o cmax2.exr")

# test clamping
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR + "/grid.tif --resize 50%"
            + " --clamp:min=0.2:max=,,0.5,1 -o grid-clamped.tif")

# test kernel
command += oiiotool ("--kernel bspline 15x15 -o bsplinekernel.exr")

# test convolve
command += oiiotool ("src/tahoe-small.tif --kernel bspline 15x15 --convolve "
            + "-d uint8 -o bspline-blur.tif")

# test blur
command += oiiotool ("src/tahoe-small.tif --blur 5x5 -d uint8 -o gauss5x5-blur.tif")

# test median filter
command += oiiotool ("src/tahoe-small.tif --median 5x5 -d uint8 -o tahoe-median.tif")

# test dilate and erode
# command += oiiotool ("--pattern constant:color=0.1,0.1,0.1 80x64 3 --text:x=8:y=54:size=40:font=DroidSerif Aai -o morphsource.tif")
command += oiiotool ("src/morphsource.tif --dilate 3x3 -d uint8 -o dilate.tif")
command += oiiotool ("src/morphsource.tif --erode 3x3 -d uint8 -o erode.tif")
# command += oiiotool ("morphsource.tif --erode 3x3 --dilate 3x3 -d uint8 -o morphopen.tif")
# command += oiiotool ("morphsource.tif --dilate 3x3 --erode 3x3 -d uint8 -o morphclose.tif")
# command += oiiotool ("morphsource.tif morphopen.tif -sub -d uint8 -o tophat.tif")
# command += oiiotool ("morphclose.tif morphsource.tif -sub -d uint8 -o bottomhat.tif")

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
command += oiiotool ("subimages-4.exr --subimage:delete=1 layerB -o subimage-noB.exr")
command += oiiotool ("subimages-2.exr --sisplit -o subimage2.exr " +
                     "--pop -o subimage1.exr")
command += oiiotool ("subimages-4.exr -cmul:subimages=0,2 0.5 -o subimage-individual.exr")

# test sequences
command += oiiotool ("src/tahoe-tiny.tif -o copyA.1-10#.jpg")
command += oiiotool (" --info  " +  " ".join(["copyA.{0:04}.jpg".format(x) for x in range(1,11)]))
command += oiiotool ("--frames 1-5 --echo \"Sequence 1-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5-5 --echo \"Sequence -5-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5--2 --echo \"Sequence -5--2:  {FRAME_NUMBER}\"")

# test expression substitution
command += oiiotool ('-echo "16+5={16+5}" -echo "16-5={16-5}" -echo "16*5={16*5}"')
command += oiiotool ('-echo "16/5={16/5}" -echo "16//5={16//5}" -echo "16%5={16%5}"')
command += oiiotool ("src/tahoe-small.tif --pattern fill:top=0,0,0,0:bottom=0,0,1,1 " +
                     "{TOP.geom} {TOP.nchannels} -d uint8 -o exprgradient.tif")
command += oiiotool ("src/tahoe-small.tif -cut '{TOP.width-20* 2}x{TOP.height-40+(4*2- 2 ) /6-1}+{TOP.x+100.5-80.5 }+{TOP.y+20}' -d uint8 -o exprcropped.tif")
command += oiiotool ("src/tahoe-small.tif -o exprstrcat{TOP.compression}.tif")
command += oiiotool ("src/tahoe-tiny.tif -subc '{TOP.MINCOLOR}' -divc '{TOP.MAXCOLOR}' -o tahoe-contraststretch.tif")
# test use of quotes inside evaluation, {TOP.foo/bar} would ordinarily want
# to interpret '/' for division, but we want to look up metadata called
# 'foo/bar'.
command += oiiotool ("-create 16x16 3 -attrib \"foo/bar\" \"xyz\" -echo \"{TOP.'foo/bar'} should say xyz\"")
command += oiiotool ("-create 16x16 3 -attrib smpte:TimeCode \"01:02:03:04\" -echo \"timecode is {TOP.'smpte:TimeCode'}\"")
# Ensure that --evaloff/--evalon work
command += oiiotool ("-echo \"{1+1}\" --evaloff -echo \"{3+4}\" --evalon -echo \"{2*2}\"")

# test --iconfig
command += oiiotool ("--info -v -metamatch Debug --iconfig oiio:DebugOpenConfig! 1 black.tif")

# test -i:ch=...
command += oiiotool ("--pattern fill:color=.6,.5,.4,.3,.2 64x64 5 -d uint8 -o const5.tif")
command += oiiotool ("-i:ch=R,G,B const5.tif -o const5-rgb.tif")

# Test that combining two images, if the first has no alpha but the second
# does, gets the right channel names instead of just copying from the first.
command += oiiotool ("-pattern constant:color=1,0,0 64x64 3 -pattern constant:color=0,1,0,1 64x64 4 -add -o add_rgb_rgba.exr")
command += info_command ("add_rgb_rgba.exr", safematch=True)


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "filled.tif",
            "autotrim.tif",
            "histogram_regular.tif", "histogram_cumulative.tif",
            "trim.tif", "trimsubimages.tif",
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
            "tahoe-filled.tif", "growholes.tif",
            "rangecompress.tif", "rangeexpand.tif",
            "rangecompress-luma.tif", "rangeexpand-luma.tif",
            "min.exr", "cmin1.exr", "cmin2.exr",
            "max.exr", "cmax1.exr", "cmax2.exr",
            "grid-clamped.tif",
            "bsplinekernel.exr", "bspline-blur.tif",
            "gauss5x5-blur.tif", "tahoe-median.tif",
            "dilate.tif", "erode.tif",
            "unsharp.tif", "unsharp-median.tif", "tahoe-laplacian.tif",
            "fft.exr", "ifft.exr",
            "polar.exr", "unpolar.exr",
            "subimages-2.exr",
            "subimages-4.exr",
            "subimageD3.exr",
            "subimageB1.exr",
            "subimage-noB.exr",
            "subimage-individual.exr",
            "subimage1.exr",
            "labeladd.exr",
            "exprgradient.tif", "exprcropped.tif", "exprstrcatlzw.tif",
            "tahoe-contraststretch.tif",
            "const5-rgb.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
