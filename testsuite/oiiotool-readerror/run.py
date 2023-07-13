#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio


# change redirection to send stderr to a separate file
redirect = " >> out.txt 2> out.err.txt "

command += oiiotool ("src/incomplete.exr -o out.exr")
failureok = 1

outputs = [ "out.txt", "out.err.txt" ]
