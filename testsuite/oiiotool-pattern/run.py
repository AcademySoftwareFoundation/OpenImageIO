#!/usr/bin/env python 

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



# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "pattern-const.tif", "pattern-gradienth.tif",
            "pattern-gradientv.tif", "pattern-gradient4.tif",
            "noise-uniform3.tif", "noise-gauss.tif", "noise-salt.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
