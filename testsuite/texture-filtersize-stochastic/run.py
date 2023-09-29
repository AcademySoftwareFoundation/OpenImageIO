#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# This test just views the "miplevels" texture straight on, but it uses
# -widthramp to smoothly blend between mipmap levels from left to right
# (wanting the 256^2 level at the left and the 64^2 level at the right).
# Using MIPmode StochasticAniso and pseudo-random variate, this tests that
# we are blending between the levels correctly.


command = testtex_command (OIIO_TESTSUITE_IMAGEDIR + "/miplevels.tx",
                           " -nowarp -res 256 256 -stochastic 1 -widthramp 4 -d uint8 -o out.tif")
outputs = [ "out.tif" ]
