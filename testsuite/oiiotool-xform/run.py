#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

#import OpenImageIO as oiio
import shutil
import os

## This testsuite entry tests oiiotool features related to image
## transformations (moving pixels around and resampling).


# helper function
def make_test_pattern1 (filename, xres=288, yres=216) :
    buf = oiio.ImageBuf (oiio.ImageSpec (xres, yres, 3, oiio.HALF))
    for y in range(yres) :
        for x in range(xres) :
            b = 0.25 + 0.5 * float (((x//16) & 1) ^ ((y//16) & 1))
            if x == 1 or y == 1 or x == xres-2 or y == yres-2 :
                b = 0.0
            if (((x >= 10 and x <= 20) or (x >= xres-20 and x <= xres-10)) and
                ((y >= 10 and y <= 20) or (y >= yres-20 and y <= yres-10))) :
                b = 0.0
            if ((x == 15 or x == xres-15) and (y == 15 or y == yres-15)) :
                b = 1.0
            buf.setpixel (x, y, (float(x)/1000.0, float(y)/1000.0, b))
    buf.write (filename)


# Create some test images we need
# No need to do this every time, we stashed it in src
#make_test_pattern1 ("src/target1.exr", 288, 216)

oiiotoolsrcdir = os.path.join(OIIO_TESTSUITE_ROOT, "oiiotool", "src")
shutil.copy (oiiotoolsrcdir + "/image.tif", "./image.tif")


# test resample
command += oiiotool ("../common/grid.tif --resample 128x128 -o resample.tif")

# test resize
command += oiiotool ("../common/grid.tif --resize 256x256 -o resize.tif")
command += oiiotool ("../common/grid.tif --resize 25% -o resize2.tif")

# test extreme resize
command += oiiotool ("../common/grid.tif --resize 64x64 -o resize64.tif")
command += oiiotool ("resize64.tif --resize 512x512 -o resize512.tif")

# test resize with non-default from/to/offset
command += oiiotool ("../common/grid.tif --resize:from=200x200+300+300 64x64 -o resizefrom.tif")
command += oiiotool ("../common/grid.tif --resize:from=200x200+300+300:to=32x32 64x64 -o resizefromto.tif")
command += oiiotool ("../common/grid.tif --resize:from=200x200+300+300:to=32x32+5-5 64x64 -o resizefromtooffset.tif")

# test resize with nonzero origin. Save to exr to make extra sure we have
# the display and data windows correct.
command += oiiotool ("--pattern fill:topleft=1,0,0:topright=0,1,0:bottomleft=0,0,1:bottomright=0,1,1 64x64 3 " +
                     "--origin +100+100 --fullsize 256x256+0+0 " +
                     "--resize 128x128 -d half -o resized-offset.exr")
# test fit
command += oiiotool ("../common/grid.tif --fit 360x240 -d uint8 -o fit.tif")
command += oiiotool ("../common/grid.tif --fit 240x360 -d uint8 -o fit2.tif")
# regression test: --fit without needing resize used to be problematic
command += oiiotool ("../common/tahoe-tiny.tif --fit 128x128 -d uint8 -o fit3.tif")
# test --fit:exact=1 when we can't get a precise whole-pixel fit of aspect
command += oiiotool ("src/target1.exr --fit:exact=1:filter=blackman-harris 216x162 -o fit4.exr")
# test the different fill modes. We do this with a test pattern image
command += oiiotool ("--pattern constant:color=0.25,0.25,0.25,1 256x128 4 "
                     "--box:color=1,1,1 0,0,255,127 --box:color=1,0,0 4,4,251,123 "
                     "-d half -o fittestw.exr")
command += oiiotool ("--pattern constant:color=0.25,0.25,0.25,1 128x256 4 "
                     "--box:color=1,1,1 0,0,127,255 --box:color=1,0,0 4,4,123,251 "
                     "-d half -o fittesth.exr")
pattern = "fittest{0}.exr --fit:exact=1:pad=1:fillmode={1} {2} -o fit{0}-{1}-{2}.exr"
for wh in [ 'w', 'h' ] :
    for mode in [ 'letterbox', 'width', 'height' ] :
        for res in [ '200x200', '300x300' ] :
            command += oiiotool (pattern.format(wh, mode, res))


# test --pixelaspect
command += oiiotool ("../common/tahoe-small.tif -resize 256x192 --pixelaspect 2.0 -d uint8 -o pixelaspect.tif")

# test rotate
command += oiiotool ("resize.tif --rotate 45 -o rotated.tif")
command += oiiotool ("resize.tif --rotate:center=50,50 45 -o rotated-offcenter.tif")
command += oiiotool ("resize.tif --rotate 45 --rotate 90 --rotate 90 --rotate 90 --rotate 45 -o rotated360.tif")

# test warp
command += oiiotool ("resize.tif --warp 0.7071068,0.7071068,0,-0.7071068,0.7071068,0,128,-53.01933,1 -o warped.tif")

# test st_warp
# We use an identity ST pattern with a bit of gamma to simulate some warping.
command += oiiotool ("resize.tif "
                     "--pattern fill:topleft=0,0,0:topright=1,0,0:bottomleft=0,1,0:bottomright=1,1,0 256x256 3 "
                     "--powc 1.2 --st_warp -o st_warped.tif")

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


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "resample.tif", "resize.tif", "resize2.tif",
            "resize64.tif", "resize512.tif",
            "resized-offset.exr",
            "resizefrom.tif", "resizefromto.tif", "resizefromtooffset.tif",
            "fit.tif", "fit2.tif", "fit3.tif", "fit4.exr",
            "fitw-letterbox-200x200.exr",
            "fitw-width-200x200.exr",
            "fitw-height-200x200.exr",
            "fith-letterbox-200x200.exr",
            "fith-width-200x200.exr",
            "fith-height-200x200.exr",
            "fitw-letterbox-300x300.exr",
            "fitw-width-300x300.exr",
            "fitw-height-300x300.exr",
            "fith-letterbox-300x300.exr",
            "fith-width-300x300.exr",
            "fith-height-300x300.exr",
            "pixelaspect.tif",
            "warped.tif",
            "st_warped.tif",
            "rotated.tif", "rotated-offcenter.tif", "rotated360.tif",
            "flip.tif", "flip-crop.tif",
            "flop.tif", "flop-crop.tif",
            "flipflop.tif", "flipflop-crop.tif",
            "rotate90.tif", "rotate90-crop.tif",
            "rotate270.tif", "rotate270-crop.tif",
            "reorient1.tif", "reorient2.tif", "reorient3.tif",
            "transpose.tif", "transpose-crop.tif",
            "cshift.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
