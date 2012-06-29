#!/usr/bin/env python 

# Test Poisson Image Editing algorithms

# smooth image completion
command += (oiio_app("oiiotool") 
            + " src_img.jpg --smoothcompletion mask.png -o out.jpg >> out.txt ;\n")

# Outputs to check against references
outputs = [ "out.jpg", "out.txt" ]