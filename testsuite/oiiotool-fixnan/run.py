#!/usr/bin/python 

command += (oiio_app ("oiiotool") + 
                     " bad.exr --fixnan black -o black.exr >> out.txt ;\n")
command += (oiio_app ("oiiotool") + 
                     " bad.exr --fixnan box3 -o box3.exr >> out.txt ;\n")
command += info_command ("bad.exr", "--stats", safematch=True)
command += info_command ("black.exr", "--stats", safematch=True)
command += info_command ("box3.exr", "--stats", safematch=True)

# Outputs to check against references
outputs = [ "black.exr", "box3.exr", "out.txt" ]
