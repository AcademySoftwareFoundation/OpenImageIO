#!/usr/bin/env python 

# Test for oiiotool application of the Porter/Duff compositing operations
#


# test flip
command += (oiio_app("oiiotool") 
            + " --flip a.exr -o flip.exr >> out.txt ;\n")

# test flop
command += (oiio_app("oiiotool")
           + " --flop a.exr -o flop.exr >> out.txt ;\n")

#test flipflop
command += (oiio_app("oiiotool")
           + " --flipflop a.exr -o flipflop.exr >> out.txt ;\n")

# Outputs to check against references
outputs = [ "flip.exr", "flop.exr", "flipflop.exr", "out.txt" ]

