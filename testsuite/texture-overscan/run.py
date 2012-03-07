#!/usr/bin/python 

command = (oiio_app("maketx") + " --filter lanczos3 "
           + parent + "/oiio-images/grid-overscan.exr"
           + " -o grid-overscan.exr ;\n")
command = command + testtex_command ("grid-overscan.exr", "--wrap black")

outputs = [ "out.exr" ]
