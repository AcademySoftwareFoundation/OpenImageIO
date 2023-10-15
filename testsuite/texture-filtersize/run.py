#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# This test verifies that the TextureSystem is sampling the correct
# MIPmap levels given the input derivatives.
#
# It uses 'testtex --filtertest', which produces an image where each
# pixel samples at the center of the source image, and keeps the minor
# axis of the filter at 1/256, but we vary the eccentricity (i.e. major
# axis length) as we go left (1) to right (32), and vary the angle as we
# go top (0) to bottom (2pi).
#
# If filtering is correct, all pixels should sample from the same MIP
# level because they have the same minor axis (1/256), regardless of
# eccentricity or angle.  If we specify a texture that has a
# distinctive color at the 256-res level, and something totally
# different at the 512 and 128 levels, it should be easy to verify that
# we aren't over-filtering or under-filtering.
#
# And, hey, we have such a texture: ../oiio-images/miplevels.tx.  This
# image is all green (except for black "256" text) on the 256-res level,
# all red at the 512 level (except for black "512" text), and all blue
# at the 128 level (except for black "128" text).  So, a correct
# behavior for this test will be to have an all-green appearance.  If
# there is any pulling in texels from other MIP levels, you will see
# non-zero blue or red channel values.  Note that we do expect the green
# level to vary a bit, because some angles and filter major axis sizes
# will pull in some of the black of the "256" text.  That's ok.  The
# important thing is no red or blue (beyond minuscule amounts that would
# be due to acceptable numerical errors in the filter estimation).
#
# Caveat: There are other kind of errors possible, not detected by this
# test.  It could be getting the eccentricity or angle wrong. But those
# errors would only affect grazing angles, it wouldn't give people the
# impression that texture was too blurry everywhere. With this test, we
# can at least be sure that our basic MIPmap level selection is dead-on.



command = testtex_command (OIIO_TESTSUITE_IMAGEDIR + "/miplevels.tx",
                           " --filtertest -res 256 256 -d uint8 -o out.tif")
outputs = [ "out.tif" ]
