#!/usr/bin/env python

command = (oiio_app("testtex") + " -res 256 256 "
           + "-texoptions max_mip_res=128 "
           + OIIO_TESTSUITE_IMAGEDIR + "/miplevels.tx"
           + " -d uint8 -o out.tif ;\n")

outputs = [ "out.tif", "out.txt" ]
