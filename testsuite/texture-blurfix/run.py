#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# This test enables the texture blur fix that provides mathematically correct 
# st-blur results instead of the legacy mode that overblur the textures.
# The default warp projection is used with the checker texture at several
# blur levels to verify that the fixed blur is consistent regardless of
# the ellipse orientation in ST space.

hardfail = 0.04

checker = "../common/textures/checker.tx"

command += testtex_command (checker, "--fix-texture-blur --blur 0.0 -d uint8 -o checker-0.00.tif")
command += testtex_command (checker, "--fix-texture-blur --blur 0.02 -d uint8 -o checker-0.02.tif")
command += testtex_command (checker, "--fix-texture-blur --blur 0.05 -d uint8 -o checker-0.05.tif")
command += testtex_command (checker, "--fix-texture-blur --blur 0.1 -d uint8 -o checker-0.10.tif")
command += testtex_command (checker, "--fix-texture-blur --blur 0.2 -d uint8 -o checker-0.20.tif")
outputs = [ "checker-0.00.tif", "checker-0.02.tif", "checker-0.05.tif",
            "checker-0.10.tif", "checker-0.20.tif" ]
