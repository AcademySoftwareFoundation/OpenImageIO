#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Read and print stats, that tests the read.
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/jpeg/ultrahdr/sky.jpg --printstats")

# Test the writer: synthesize a linear Rec.709 HDR image (with values
# well above 1.0), write it as an Ultra HDR JPEG, then read it back.
command += oiiotool ("--create 64x64 3 "
                     + "--pattern fill:top=0,0,0:bottom=8,4,2 64x64 3 "
                     + "--attrib oiio:ColorSpace lin_rec709_scene "
                     + "-d half --attrib jpeg:ultrahdr 1 "
                     + "-o uhdr_write.jpg")
command += oiiotool ("--info uhdr_write.jpg")

# Confirm the writer rejects unsuitable inputs.
command += oiiotool ("--create 64x64 3 --attrib oiio:ColorSpace lin_rec709_scene "
                     + "-d uint8 --attrib jpeg:ultrahdr 1 -o uhdr_bad_uint8.jpg",
                     failureok = True)
command += oiiotool ("--create 64x64 3 --attrib oiio:ColorSpace lin_ap1_scene "
                     + "-d half --attrib jpeg:ultrahdr 1 -o uhdr_bad_colorspace.jpg",
                     failureok = True)
