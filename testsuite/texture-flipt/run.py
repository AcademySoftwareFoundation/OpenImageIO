#!/usr/bin/env python

command = oiiotool ("-pattern fill:topleft=0,0,0:topright=1,0,0:bottomleft=0,1,0:bottomright=1,1,1 "
                    + "64x64 3 -otex gradient.tx")
command += testtex_command ("gradient.tx",
                           "-flipt -nowarp -derivs -res 64 64 -d half -o out.exr")
outputs = [ "out.exr" ]
