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
# A malformed (too-short) graphics-control extension sub-block must be
# dropped without an out-of-bounds read; the 1x1 image still decodes.
command += info_command ("src/short-graphics-ext.gif", hash=False)

outputs = [ "tahoe-tiny.gif", "out.txt" ]

# Subimage seeking. GIF frames are drawn onto a shared canvas and giflib
# only reads forward, so reaching an earlier frame means reopening the file
# and replaying the frames before it. A 4-frame animation with one solid
# color per frame lets a read after a seek say unambiguously which frame we
# landed on.
command += oiiotool ("--pattern fill:color=1,0,0 8x8 3 "
                     "--pattern fill:color=0,1,0 8x8 3 "
                     "--pattern fill:color=0,0,1 8x8 3 "
                     "--pattern fill:color=1,1,0 8x8 3 "
                     "--siappendall -o frames.gif")

# Seeking around needs to inspect the reader's state, not just its pixels,
# so that part is written in Python. Skip it where the bindings aren't
# available rather than making the whole GIF test depend on them.
if subprocess.call ([pythonbin, "-c", "import OpenImageIO"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL) == 0 :
    redirect_push ("seek.txt")
    command += pythonbin + " src/test_gif_seek.py " + redirect + " ;"
    redirect_pop ()
