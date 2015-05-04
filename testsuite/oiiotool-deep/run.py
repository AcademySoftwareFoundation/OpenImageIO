#!/usr/bin/env python 

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

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [ "flat.exr",
            "ch.exr",
            "deepen.exr",
            "crop1.exr", "crop2.exr",
            "trim1.exr", "trim2.exr",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
