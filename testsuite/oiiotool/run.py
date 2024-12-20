#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect += " 2>&1"
failureok = True

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
command += oiiotool ("../common/tahoe-small.tif --rangecompress -d uint8 -o rangecompress.tif")
command += oiiotool ("rangecompress.tif --rangeexpand -d uint8 -o rangeexpand.tif")
command += oiiotool ("../common/tahoe-small.tif --rangecompress:luma=1 -d uint8 -o rangecompress-luma.tif")
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
            
# Test -- scale
command += oiiotool ("--pattern fill:topleft=0,0,1:topright=0,1,0:bottomleft=1,0,1:bottomright=1,1,0 64x64 3"
            + " --pattern fill:top=0:bottom=1 64x64 1"
            + " --scale -o scale.exr")

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
command += oiiotool ("../common/tahoe-small.tif --invert -o invert.tif")

# Test --powc val (raise all channels by the same power)
command += oiiotool ("grey128.exr --powc 2 -o cpow1.exr")
# Test --powc val,val,val... (per-channel powers)
command += oiiotool ("grey128.exr --powc 2,2,1 -o cpow2.exr")

# Test --normalize
command += oiiotool ("src/norm.exr --normalize -o normalize.exr " +
                     "src/norm.exr --normalize:scale=0.5 -o normalize_scale.exr " +
                     "src/normoffset.exr --normalize:incenter=0.5 -o normalize_offsetin.exr " +
                     "src/norm.exr --normalize:outcenter=0.5:scale=0.5 -o normalize_offsetscaleout.exr " +
                     "src/normoffset.exr --normalize:incenter=0.5:outcenter=0.5:scale=0.5 -o normalize_offsetscale.exr ")


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
command += oiiotool ("../common/tahoe-small.tif --chsum:weight=.2126,.7152,.0722 "
            + "-d uint8 -o chsum.tif")

# test --trim
command += oiiotool ("--create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 "
                     + " -rotate 30 -trim -origin +0+0 -fullpixels -d uint8 -o trim.tif")

# test --trim, tricky case of multiple subimages
command += oiiotool (  "-a --create 320x240 3 -fill:color=.1,.5,.1 120x80+50+70 -rotate 30 "
                     + "--create 320x240 3 -fill:color=.5,.5,.1 100x10+70+70 -rotate 140 "
                     + "--siappend -trim -origin +0+0 -fullpixels -d uint8 -o trimsubimages.tif")

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

# test --maxchan, --minchan
command += oiiotool ("--pattern fill:topleft=0,0,0.2:topright=1,0,0.2:bottomleft=0,1,0.2:bottomright=1,1,0.2 100x100 3 " +
                        " --maxchan -d uint8 -o maxchan.tif")
command += oiiotool ("--pattern fill:topleft=0,0,0.8:topright=1,0,0.8:bottomleft=0,1,0.8:bottomright=1,1,0.8 100x100 3 " +
                        " --minchan -d uint8 -o minchan.tif")

# test clamping
command += oiiotool ("../common/grid.tif --resize 50%"
            + " --clamp:min=0.2:max=,,0.5,1 -o grid-clamped.tif")

# test kernel
command += oiiotool ("--kernel bspline 15x15 -o bsplinekernel.exr")

# test convolve
command += oiiotool ("../common/tahoe-small.tif --kernel bspline 15x15 --convolve "
            + "-d uint8 -o bspline-blur.tif")

# test blur
command += oiiotool ("../common/tahoe-small.tif --blur 5x5 -d uint8 -o gauss5x5-blur.tif")

# test median filter
command += oiiotool ("../common/tahoe-small.tif --median 5x5 -d uint8 -o tahoe-median.tif")

# test dilate and erode
# command += oiiotool ("--pattern constant:color=0.1,0.1,0.1 80x64 3 --text:x=8:y=54:size=40:font=DroidSerif Aai -o morphsource.tif")
command += oiiotool ("src/morphsource.tif --dilate 3x3 -d uint8 -o dilate.tif")
command += oiiotool ("src/morphsource.tif --erode 3x3 -d uint8 -o erode.tif")
# command += oiiotool ("morphsource.tif --erode 3x3 --dilate 3x3 -d uint8 -o morphopen.tif")
# command += oiiotool ("morphsource.tif --dilate 3x3 --erode 3x3 -d uint8 -o morphclose.tif")
# command += oiiotool ("morphsource.tif morphopen.tif -sub -d uint8 -o tophat.tif")
# command += oiiotool ("morphclose.tif morphsource.tif -sub -d uint8 -o bottomhat.tif")

# test unsharp mask
command += oiiotool ("../common/tahoe-small.tif --unsharp -d uint8 -o unsharp.tif")

# test unsharp mask with median filter
command += oiiotool ("../common/tahoe-small.tif --unsharp:kernel=median -d uint8 -o unsharp-median.tif")

# test laplacian
command += oiiotool ("../common/tahoe-tiny.tif --laplacian -d uint8 -o tahoe-laplacian.tif")

# test fft, ifft
command += oiiotool ("../common/tahoe-tiny.tif --ch 2 --fft -d float -o fft.exr")
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
command += oiiotool ('-echo "This should make an error:" ' +
                     '--create 1x1 3 --label 2hot2handle -o out.tif')

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

# Test --printstats
command += oiiotool ("../common/tahoe-tiny.tif --echo \"--printstats:\" --printstats:native=1")
command += oiiotool ("../common/tahoe-tiny.tif --printstats:natve=1:window=10x10+50+50 --echo \" \"")

# test --iconfig
command += oiiotool ("--info -v -metamatch Debug --iconfig oiio:DebugOpenConfig! 1 " +
                     "--iconfig:type=float oiio:DebugOpenConfigInt! 2 " +
                     "--iconfig:type=float oiio:DebugOpenConfigFloat! 3 " +
                     "--iconfig:type=string oiio:DebugOpenConfigStr! 4 " +
                     "black.tif")

# test -i:ch=...
command += oiiotool ("--pattern fill:color=.6,.5,.4,.3,.2 64x64 5 -d uint8 -o const5.tif")
command += oiiotool ("-i:ch=R,G,B const5.tif -o const5-rgb.tif")

# Test that combining two images, if the first has no alpha but the second
# does, gets the right channel names instead of just copying from the first.
command += oiiotool ("-pattern constant:color=1,0,0 64x64 3 -pattern constant:color=0,1,0,1 64x64 4 -add -o add_rgb_rgba.exr")
command += info_command ("add_rgb_rgba.exr", safematch=True)

# Test --missingfile
command += oiiotool ("--create 320x240 4 --box:color=1,0,0,1:fill=1  10,10,200,100 -d uint8 -o box.tif")
# Test again using --missingfile black
command += oiiotool ("--missingfile black box.tif missing.tif --over -o box_over_missing2.tif || true")
# Test again using --missingfile checker
command += oiiotool ("--missingfile checker box.tif missing.tif --over -o box_over_missing3.tif || true")

# Test --dumpdata
command += oiiotool ("--pattern fill:left=0,0,0:right=1,1,0 2x2 3 -d half -o dump.exr")
command += oiiotool ("-echo dumpdata: --dumpdata dump.exr")
command += oiiotool ("-echo dumpdata:C --dumpdata:C=data dump.exr")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "filled.tif",
            "autotrim.tif",
            "trim.tif", "trimsubimages.tif",
            "add.exr", "cadd1.exr", "cadd2.exr",
            "sub.exr", "subc.exr",
            "mul.exr", "cmul1.exr", "cmul2.exr",
            "div.exr", "divc1.exr", "divc2.exr",
            "mad.exr", "invert.tif",
            "cpow1.exr", "cpow2.exr",
            "normalize.exr", "normalize_scale.exr", "normalize_offsetin.exr",
            "normalize_offsetscaleout.exr", "normalize_offsetscale.exr",
            "abs.exr", "absdiff.exr", "absdiffc.exr",
            "chsum.tif",
            "tahoe-filled.tif", "growholes.tif",
            "rangecompress.tif", "rangeexpand.tif",
            "rangecompress-luma.tif", "rangeexpand-luma.tif",
            "min.exr", "cmin1.exr", "cmin2.exr",
            "max.exr", "cmax1.exr", "cmax2.exr",
            "maxchan.tif", "minchan.tif",
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
            "const5-rgb.tif",
            "box_over_missing2.tif",
            "box_over_missing3.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
