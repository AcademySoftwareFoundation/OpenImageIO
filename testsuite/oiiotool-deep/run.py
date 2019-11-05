#!/usr/bin/env python 

exrdir = OIIO_TESTSUITE_IMAGEDIR + "/v2/LowResLeftView"

# test --flatten : turn deep into composited non-deep
command += oiiotool("src/deepalpha.exr --flatten -o flat.exr")

# test --ch on deep files (and --chnames)
command += oiiotool("src/deepalpha.exr --ch =0.0,A,=0.5,A,Z --chnames R,G,B,A,Z --flatten -d half -o ch.exr")

# --deepen
command += oiiotool("-pattern fill:topleft=0,14:topright=0.5,15:bottomleft=0.5,14:bottomright=1,15 4x4 2 -chnames A,Z -fill:color=0,1e38 2x1+1+2 -o az.exr")
command += oiiotool("az.exr -deepen -o deepen.exr")

# --crop deep images
command += oiiotool("deepen.exr -crop 2x2+1+1 -o crop1.exr")
command += oiiotool("deepen.exr -crop 5x5+1+1 -o crop2.exr")

# --trim
command += oiiotool("crop2.exr -trim -o trim1.exr")
command += oiiotool("-autotrim crop2.exr -o trim2.exr")

# --addc
command += oiiotool("src/deepalpha.exr -addc 0,10 -crop 160x100 -o deep_addc.exr")
# --subc
command += oiiotool("src/deepalpha.exr -subc 0,-1 -crop 160x105 -o deep_subc.exr")
# --mulc
command += oiiotool("src/deepalpha.exr -mulc 1,10 -crop 160x110 -o deep_mulc.exr")
# --divc
command += oiiotool("src/deepalpha.exr -divc 1,2 -crop 160x115 -o deep_divc.exr")

# --deepmerge
command += oiiotool (exrdir+"/Balls.exr -cut 512x288+0+0 " +
                     exrdir+"/Ground.exr -cut 512x288+0+0 " +
                     exrdir+"/Leaves.exr -cut 512x288+0+0 " +
                     exrdir+"/Trunks.exr -cut 512x288+0+0 " +
                     " -deepmerge -deepmerge -deepmerge -flatten " +
                     " -ch R,G,B,A -d half -o deepmerge.exr")

# --resample
command += oiiotool (exrdir+"/Balls.exr -resample 128x72 -o resampled-balls.exr")



# Regression test: it used to be that comparing deep image, it would loop
# only over A's samples, so if A had no samples in a pixel but B had
# samples, then the comparison would fail to see a difference. Be sure to
# test both orderings.
# Compare with both orderings
command += oiiotool ("--fail 100 src/deep-nosamples.exr src/deep-onesample.exr --diff")
command += oiiotool ("--fail 100 src/deep-onesample.exr src/deep-nosamples.exr --diff")
 # Recipe for creating the files:
 # spec = oiio.ImageSpec (1, 1, 1, oiio.TypeDesc.TypeFloat)
 # spec.channelnames = ("Z")
 # spec.deep = True
 # buf = oiio.ImageBuf (spec)
 # buf.write ("src/deep-nosamples.exr")  # write a deep image with no samples
 # buf.set_deep_samples (0, 0, 0, 1)
 # buf.set_deep_value (0, 0, 0, 0, 0, 42.0)
 # buf.write ("src/deep-onesample.exr")  # write another deep image with 1 sample


# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "flat.exr",
            "ch.exr",
            "deepen.exr",
            "crop1.exr", "crop2.exr",
            "trim1.exr", "trim2.exr",
            "deep_addc.exr",
            "deep_subc.exr",
            "deep_mulc.exr",
            "deep_divc.exr",
            "deepmerge.exr",
            "resampled-balls.exr",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
