#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1 "

files = ["gif_animation.gif", "gif_oiio_logo_with_alpha.gif",
         "gif_tahoe.gif", "gif_tahoe_interlaced.gif",
         "gif_bluedot.gif", "gif_diagonal_interlaced.gif",
         "gif_triangle_interlaced.gif", "gif_test_disposal_method.gif",
         "gif_test_loop_count.gif", "gif_transparent_rgb.gif"]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# Test write / conversion to GIF
command += oiiotool (OIIO_TESTSUITE_ROOT+"/common/tahoe-tiny.tif -o tahoe-tiny.gif")
command += info_command ("tahoe-tiny.gif")

# Regression tests
command += oiiotool ("-nostderr -oiioattrib try_all_readers 0 src/crash_4163.gif -o test.exr", failureok = True)
command += info_command ("src/gif_idx_overflow_32768x16385_top16384_1x1.gif",
                         extraargs="-oiioattrib try_all_readers 0",
                         verbose=False, hash=False, failureok=True)

outputs = [ "tahoe-tiny.gif", "out.txt" ]
