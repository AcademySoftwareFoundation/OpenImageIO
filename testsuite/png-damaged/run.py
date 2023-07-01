#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio


# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

command += rw_command (OIIO_TESTSUITE_IMAGEDIR + "/png/broken",
                       "invalid_gray_alpha_sbit.png",
                       printinfo=False)
