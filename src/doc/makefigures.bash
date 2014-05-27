#!/bin/bash

oiiotool ../../../../oiio-images/tahoe-gps.jpg --colorconvert sRGB linear --resize 320x240 --colorconvert linear sRGB -o tahoe-small.jpg
oiiotool ../../../../oiio-images/grid.tif --resize 256x256 --colorconvert linear sRGB -o grid-small.jpg

oiiotool tahoe-small.jpg --tocolorspace linear --cadd 0.2 --tocolorspace sRGB -o cadd.jpg
oiiotool tahoe-small.jpg --tocolorspace linear --cmul 0.5 --tocolorspace sRGB -o cmul.jpg
oiiotool tahoe-small.jpg --tocolorspace linear --chsum:weight=.2126,.7152,.0722 --ch 0,0,0 --tocolorspace sRGB -o luma.jpg
oiiotool grid-small.jpg --flip -o flip.jpg
oiiotool grid-small.jpg --flop -o flop.jpg
oiiotool grid-small.jpg --flipflop -o flipflop.jpg
oiiotool grid-small.jpg --transpose -o transpose.jpg
oiiotool grid-small.jpg --cshift +70+30 -o cshift.jpg
oiiotool --pattern constant:color=1,0.7,0.7 320x240 3 --fill:color=1,0,0 50x100+50+75 --tocolorspace sRGB -o fill.jpg
oiiotool --pattern checker:color1=0.1,0.1,0.1:color2=0.4,0.4,0.4:width=32:height=32 320x240 3 --tocolorspace sRGB -o checker.jpg
oiiotool --create 320x240 3 --text:x=25:y=50 "Hello, world" \
            --text:x=50:y=100:font="Arial Bold":color=1,0,0:size=30 "Go Big Red!" --tocolorspace sRGB -o text.jpg
#oiiotool ../../../testsuite/oiiotool/tahoe-small.tif

