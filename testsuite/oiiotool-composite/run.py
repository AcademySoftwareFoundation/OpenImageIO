#!/usr/bin/env python 

# Test for oiiotool application of the Porter/Duff compositing operations
#


# test over
command += (oiio_app("oiiotool") 
            + " a.exr --over b.exr -o a_over_b.exr >> out.txt ;\n")

# FIXME: no test for zover

# future: test in, out, etc., the other Porter/Duff operations

# Outputs to check against references
outputs = [ "a_over_b.exr", "out.txt" ]

