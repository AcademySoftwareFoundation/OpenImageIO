#!/usr/bin/env python

# This file has a corrupted Exif block in the metadata. It used to
# crash on some platforms, on others would be caught by address sanitizer.
# Fixed by #1635. This test serves to guard against regressions.
command += info_command ("src/corrupt-exif.jpg", safematch=True)

# Checking the error output is important for this test
outputs = [ "out.txt" ]
failureok = 1
