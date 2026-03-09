#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Test the mathematically correct texture blur and the legacy_texture_blur mode
# The legacy mode overblur the texture, but it is important to maintain it for backwards compatibility.

hardfail = 0.04

checker = "../common/textures/checker.tx"

# Test mathematically correct blur with various blur levels (on by default in testtex)
command += testtex_command(checker, "--blur 0.0 -d uint8 -o checker-0.00.tif")
command += testtex_command(checker, "--blur 0.02 -d uint8 -o checker-0.02.tif")
command += testtex_command(checker, "--blur 0.05 -d uint8 -o checker-0.05.tif")
command += testtex_command(checker, "--blur 0.1 -d uint8 -o checker-0.10.tif")
command += testtex_command(checker, "--blur 0.2 -d uint8 -o checker-0.20.tif")
outputs = [ "checker-0.00.tif", "checker-0.02.tif", "checker-0.05.tif",
            "checker-0.10.tif", "checker-0.20.tif" ]

# Test legacy but wrong blur with various blur levels for backwards compatibility
command += testtex_command(checker, "--blur 0.0 -d uint8 --legacy-texture-blur -o checker-lgb-0.00.tif")
command += testtex_command(checker, "--blur 0.02 -d uint8 --legacy-texture-blur -o checker-lgb-0.02.tif")
command += testtex_command(checker, "--blur 0.05 -d uint8 --legacy-texture-blur -o checker-lgb-0.05.tif")
command += testtex_command(checker, "--blur 0.1 -d uint8 --legacy-texture-blur -o checker-lgb-0.10.tif")
command += testtex_command(checker, "--blur 0.2 -d uint8 --legacy-texture-blur -o checker-lgb-0.20.tif")
outputs = [ "checker-lgb-0.00.tif", "checker-lgb-0.02.tif", "checker-lgb-0.05.tif",
            "checker-lgb-0.10.tif", "checker-lgb-0.20.tif" ]
