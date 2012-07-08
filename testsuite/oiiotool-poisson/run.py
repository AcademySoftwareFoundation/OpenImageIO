#!/usr/bin/env python 

# Test Poisson Image Editing algorithms

# smooth image completion
command += (oiio_app("oiiotool") 
            + " src_img.jpg --smoothcompletion mask.png -o out.jpg >> out.txt ;\n")

# seamless cloning
command += (oiio_app("oiiotool") 
            + " cloning_src1.png --clone cloning_src2.png cloning_mask.png -o cloning_out.jpg >> cloning_out.txt ;\n")


# Outputs to check against references
outputs = [ "out.jpg", "out.txt", "cloning_out.jpg", "cloning_out.txt" ]