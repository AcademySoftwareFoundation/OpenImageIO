#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


####################################################################
# This test exercises cryptomatte functionality
####################################################################

# Capture error output
redirect = " >> out.txt 2>&1 "

# oiiotool --cryptomatte-colors
command += oiiotool("src/cryptoasset.exr --cryptomatte-colors uCryptoAsset -o cmcolors.exr")



# Outputs to check against references
outputs = [
            "cmcolors.exr",
            "out.txt"
          ]
