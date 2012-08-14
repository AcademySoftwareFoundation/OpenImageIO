#!/usr/bin/env python 

# Test Poisson Image Editing algorithms

# smooth image completion
command += (oiio_app("oiiotool") 
            + " src_img.jpg --smoothcompletion mask.png -o out.jpg >> out.txt ;\n")

# seamless cloning
command += (oiio_app("oiiotool") 
            + " cloning_src1.png --clone cloning_mask.png cloning_src2.png -o cloning_out.jpg >> cloning_out.txt ;\n")

# seamless cloning with mixed gradients
command += (oiio_app("oiiotool") 
            + " cloning_src1.png --mixclone cloning_mask.png cloning_src2.png -o mixclone_out.jpg >> mixclone_out.txt ;\n")

# local illumination change
command += (oiio_app("oiiotool") 
            + " illum_in.jpg --illumchange illum_mask.png -o illum_out.jpg >> illum_out.txt ;\n")


# Outputs to check against references
outputs = [ "out.jpg", "out.txt", "cloning_out.jpg", "cloning_out.txt", "mixclone_out.jpg", "mixclone_out.txt", "illum_out.jpg", "illum_out.txt" ]