#!/usr/bin/env bash

if [ "${OIIOTOOL}" == "" ] ; then
    OIIOTOOL=oiiotool
fi

echo "Using OIIOTOOL=${OIIOTOOL}"

# Uncomment to make these files. But we check them in so that the figures
# can be generated without oiio-images.
#${OIIOTOOL} ../../../../oiio-images/tahoe-gps.jpg --colorconvert sRGB linear --resize 320x240 --colorconvert linear sRGB -o tahoe-small.jpg
#${OIIOTOOL} ../../../../oiio-images/grid.tif --resize 256x256 --colorconvert linear sRGB -o grid-small.jpg

${OIIOTOOL} tahoe-small.jpg --tocolorspace linear --addc 0.2 --tocolorspace sRGB -o addc.jpg
${OIIOTOOL} tahoe-small.jpg --tocolorspace linear --mulc 0.5 --tocolorspace sRGB -o mulc.jpg
${OIIOTOOL} tahoe-small.jpg --tocolorspace linear --chsum:weight=.2126,.7152,.0722 --ch 0,0,0 --tocolorspace sRGB -o luma.jpg
${OIIOTOOL} grid-small.jpg --flip -o flip.jpg
${OIIOTOOL} grid-small.jpg --flop -o flop.jpg
${OIIOTOOL} grid-small.jpg --flipflop -o flipflop.jpg
${OIIOTOOL} grid-small.jpg --rotate90 -o rotate90.jpg
${OIIOTOOL} grid-small.jpg --rotate180 -o rotate180.jpg
${OIIOTOOL} grid-small.jpg --rotate270 -o rotate270.jpg
${OIIOTOOL} grid-small.jpg --transpose -o transpose.jpg
${OIIOTOOL} grid-small.jpg --rotate 45 -o rotate45.jpg
${OIIOTOOL} grid-small.jpg --cshift +70+30 -o cshift.jpg
${OIIOTOOL} --pattern fill:bottom=1,0.7,0.7:top=1,0,0  320x240 3 --fill:color=1,0,0 50x100+50+75 --tocolorspace sRGB -o fill.jpg
#${OIIOTOOL} --create 320x240 3 -fill:bottom=1,0.7,0.7:top=1,0,0 320x240 --tocolorspace sRGB -o fill2.jpg
${OIIOTOOL} --pattern checker:color1=0.1,0.1,0.1:color2=0.4,0.4,0.4:width=32:height=32 320x240 3 --tocolorspace sRGB -o checker.jpg
${OIIOTOOL} --pattern fill:top=0.1,0.1,0.1:bottom=0,0,0.75 320x240 3 --tocolorspace sRGB -o gradient.jpg
${OIIOTOOL} --pattern fill:left=0.1,0.1,0.1:right=0,0.75,0 320x240 3 --tocolorspace sRGB -o gradienth.jpg
${OIIOTOOL} --pattern fill:topleft=.1,.1,.1:topright=1,0,0:bottomleft=0,1,0:bottomright=0,0,1 320x240 3 --tocolorspace sRGB -o gradient4.jpg
${OIIOTOOL} --pattern checker:width=16:height=16 256x256 3 --tocolorspace sRGB -o checker.jpg
${OIIOTOOL} --pattern noise:type=uniform:min=0:max=1:mono=1 256x256 3 --tocolorspace sRGB -o unifnoise1.jpg
${OIIOTOOL} --pattern noise:type=uniform:min=0:max=1 256x256 3 --tocolorspace sRGB -o unifnoise3.jpg
${OIIOTOOL} --pattern noise:type=gaussian:mean=0.5:stddev=0.2 256x256 3 --tocolorspace sRGB -o gaussnoise.jpg
${OIIOTOOL} tahoe-small.jpg --noise:type=gaussian:mean=0:stddev=0.1 -o tahoe-gauss.jpg
${OIIOTOOL} tahoe-small.jpg --noise:type=salt:portion=0.01:value=0:mono=1 -o tahoe-pepper.jpg
${OIIOTOOL} tahoe-small.jpg --blur 7x7 -o tahoe-blur.jpg
${OIIOTOOL} tahoe-pepper.jpg --median 3x3 -o tahoe-pepper-median.jpg
${OIIOTOOL} tahoe-small.jpg --laplacian -mulc 2 -o tahoe-laplacian.jpg
${OIIOTOOL} --create 320x240 3 --text:x=25:y=50 "Hello, world" \
            --text:x=50:y=100:font="Arial Bold":color=1,0,0:size=30 "Go Big Red!" \
            --tocolorspace sRGB -o text.jpg
${OIIOTOOL} --create 320x240 3 --text:x=160:y=120:xalign=center:yalign=center:size=40 "Centered" \
            --tocolorspace sRGB -o textcentered.jpg
${OIIOTOOL} tahoe-small.jpg \
            --text:x=160:y=40:xalign=center:size=40:shadow=0 "shadow = 0" \
            --text:x=160:y=80:xalign=center:size=40:shadow=1 "shadow = 1" \
            --text:x=160:y=120:xalign=center:size=40:shadow=2 "shadow = 2" \
            --tocolorspace sRGB -o textshadowed.jpg
${OIIOTOOL} tahoe-small.jpg --crop 100x120+35+40 \
            -ch R,G,B,A=1.0 -create 320x240 4 -fill:color=0.75,0.75,0.75,1 320x240 \
            -fill:color=1,1,1,1 318x238+1+1 -over -tocolorspace sRGB -o crop.jpg
${OIIOTOOL} tahoe-small.jpg --cut 100x120+35+40 -tocolorspace sRGB -o cut.jpg
${OIIOTOOL} -create 320x240 4 -fill:color=.1,.5,.1 120x80+50+70 -rotate 30 -o pretrim.jpg \
            -trim -ch R,G,B,A=1.0 -create 320x240 4 -fill:color=0.75,0.75,0.75,1 320x240 \
            -fill:color=1,1,1,1 318x238+1+1 -over -tocolorspace sRGB -o trim.jpg
${OIIOTOOL} --autocc tahoe-small.jpg --invert -o invert.jpg
${OIIOTOOL} --pattern checker:color1=.1,.2,.1:color2=.2,.2,.2 320x240 3 \
            --line:color=1,0,0 10,60,250,20,100,190 -d uint8 -o lines.png
${OIIOTOOL} --pattern checker:color1=.1,.2,.1:color2=.2,.2,.2 320x240 3 \
            --box:color=0,1,1,1 150,100,240,180 \
            --box:color=0.5,0.5,0,0.5:fill=1 100,50,180,140 \
            --colorconvert linear sRGB -d uint8 -o box.png

${OIIOTOOL} --autocc --pattern constant:color=0.1,0.1,0.1 80x64 3 --text:x=8:y=54:size=40:font=DroidSerif Aai -o morphsource.jpg
${OIIOTOOL} --autocc morphsource.jpg --dilate 3x3 -d uint8 -o dilate.jpg
${OIIOTOOL} --autocc morphsource.jpg --erode 3x3 -d uint8 -o erode.jpg
${OIIOTOOL} --autocc morphsource.jpg --erode 3x3 --dilate 3x3 -d uint8 -o morphopen.jpg
${OIIOTOOL} --autocc morphsource.jpg --dilate 3x3 --erode 3x3 -d uint8 -o morphclose.jpg
${OIIOTOOL} --autocc dilate.jpg erode.jpg -sub -d uint8 -o morphgradient.jpg
${OIIOTOOL} --autocc morphsource.jpg morphopen.jpg -sub -d uint8 -o tophat.jpg
${OIIOTOOL} --autocc morphclose.jpg morphsource.jpg -sub -d uint8 -o bottomhat.jpg

${OIIOTOOL} -autocc tahoe-small.jpg --contrast:black=0.1:white=0.75 -o tahoe-lincontrast.jpg
${OIIOTOOL} -autocc tahoe-small.jpg --contrast:black=1:white=0 -o tahoe-inverse.jpg
${OIIOTOOL} -autocc tahoe-small.jpg --contrast:scontrast=5 -o tahoe-sigmoid.jpg

${OIIOTOOL} -autocc tahoe-small.jpg --colormap inferno -o colormap-inferno.jpg
${OIIOTOOL} -autocc tahoe-small.jpg --colormap viridis -o colormap-viridis.jpg
${OIIOTOOL} -autocc tahoe-small.jpg --colormap turbo -o colormap-turbo.jpg
${OIIOTOOL} -autocc tahoe-small.jpg --colormap ".25,.25,.25,0,.5,0,1,0,0" -o colormap-custom.jpg

${OIIOTOOL} -autocc tahoe-small.jpg --ccmatrix "0.805,0.506,-0.311,0,-0.311,0.805,0.506,0,0.506,-0.311,0.805,0,0,0,0,1" -o tahoe-ccmatrix.jpg

#${OIIOTOOL} ../../../testsuite/oiiotool/tahoe-small.tif

