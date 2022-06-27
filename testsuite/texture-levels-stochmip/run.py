#!/usr/bin/env python

# Adjust error thresholds a tad to account for platform-to-platform variation
# in some math precision.
hardfail = 0.16
failpercent = 0.001
allowfailures = 1

command = testtex_command ("../oiio-images/miplevels.tx",
                           extraargs = "-stochastic 1 -bluenoise -d uint8 -o out.tif")
outputs = [ "out.tif" ]
