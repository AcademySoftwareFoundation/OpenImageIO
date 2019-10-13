#!/usr/bin/env python

files = ["gif_animation.gif", "gif_oiio_logo_with_alpha.gif",
         "gif_tahoe.gif", "gif_tahoe_interlaced.gif",
         "gif_bluedot.gif", "gif_diagonal_interlaced.gif",
         "gif_triangle_interlaced.gif", "gif_test_disposal_method.gif",
         "gif_test_loop_count.gif"]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# Test write / conversion to GIF
command += oiiotool (OIIO_TESTSUITE_ROOT+"/oiiotool/src/tahoe-tiny.tif -o tahoe-tiny.gif")
command += info_command ("tahoe-tiny.gif")
