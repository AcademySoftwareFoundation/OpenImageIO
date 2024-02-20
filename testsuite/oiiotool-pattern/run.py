#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# test --create
command += oiiotool ("--create 320x240 3 -d uint8 -o black.tif")
command += oiiotool ("--stats black.tif")

# test --pattern constant
command += oiiotool ("--pattern constant:color=.1,.2,.3,1 320x240 4 -o constant.tif")
command += oiiotool ("--stats constant.tif")

# test --pattern noise
command += oiiotool ("--pattern noise:type=uniform:min=0.25:max=0.75 64x64 3 -d uint8 -o noise-uniform3.tif")
command += oiiotool ("--pattern noise:type=gaussian:mean=0.5:stddev=0.1 64x64 3 -d uint8 -o noise-gauss.tif")
command += oiiotool ("--pattern noise:type=salt:portion=0.01:value=1 64x64 3 -d uint8 -o noise-salt.tif")

# test --pattern fill
command += oiiotool ("--pattern fill:color=0,0,0.5 64x64 3 -d uint8 -o pattern-const.tif")
command += oiiotool ("--pattern fill:top=0.1,0.1,0.1:bottom=0,0,0.5 64x64 3 -d uint8 -o pattern-gradientv.tif")
command += oiiotool ("--pattern fill:left=0.1,0.1,0.1:right=0,0.5,0 64x64 3 -d uint8 -o pattern-gradienth.tif")
command += oiiotool ("--pattern fill:topleft=0.1,0.1,0.1:topright=0,0.5,0:bottomleft=0.5,0,0:bottomright=0,0,0.5 64x64 3 -d uint8 -o pattern-gradient4.tif")

# test --fill
command += oiiotool ("--create 256x256 3 --fill:color=1,.5,.5 256x256 --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif")
command += oiiotool ("--create 64x64 3 --fill:top=0.1,0.1,0.1:bottom=0,0,0.5 64x64 -d uint8 -o fillv.tif")
command += oiiotool ("--create 64x64 3 --fill:left=0.1,0.1,0.1:right=0,0.5,0 64x64 -d uint8 -o fillh.tif")
command += oiiotool ("--create 64x64 3 --fill:topleft=0.1,0.1,0.1:topright=0,0.5,0:bottomleft=0.5,0,0:bottomright=0,0,0.5 64x64 -d uint8 -o fill4.tif")

# test --line
command += oiiotool ("--pattern checker:color1=.1,.1,.1:color2=0,0,0 256x256 3 " +
                     "-line:color=0.25,0,0,0.25 10,60,250,20 " +
                     "-line:color=0.5,0,0,0.5   10,62,250,100 " +
                     "-line:color=1,0,0,1       10,64,250,400 " +
                     "-line:color=0,1,0,1       250,100,10,184 " +
                     "-line:color=0,0.5,0,0.5   250,200,10,182 " +
                     "-line:color=0,0.25,0,0.25 100,400,10,180 " +
                     "-line:color=.5,.5,0,0.5  100,100,120,100,120,100,120,120,120,120,100,120,100,120,100,100 " +
                     "-box:color=0,0.5,0.5,0.5  150,100,240,180 " +
                     "-d uint8 -o lines.tif")

# test --box
command += oiiotool ("--pattern checker:color1=.1,.1,.1:color2=0,0,0 256x256 3 " +
                     "--box:color=0,1,1,1 150,100,240,180 " +
                     "--box:color=0.5,0.5,0,0.5:fill=1 100,50,180,140  " +
                     "-d uint8 -o box.tif")

# test --point
command += oiiotool ("--create 64x64 3 " +
                     "--point:color=0,1,1,1 50,10 " +
                     "--point:color=1,0,1,1 20,20,30,30,40,40 " +
                     "-d uint8 -o points.tif")



# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "pattern-const.tif", "pattern-gradienth.tif",
            "pattern-gradientv.tif", "pattern-gradient4.tif",
            "noise-uniform3.tif", "noise-gauss.tif", "noise-salt.tif",
            "filled.tif", "fillh.tif", "fillv.tif", "fill4.tif",
            "lines.tif", "box.tif", "points.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
