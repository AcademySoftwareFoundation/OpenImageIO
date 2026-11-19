#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# save the error output
redirect = " >> out.txt 2>&1 "

files = [ "checker.cin" ]
for f in files:
    command += info_command ("../oiio-images/cineon/" + f)


# Regression tests for broken files
command += info_command ("src/broken_bitdepth.cin", verbose=False, failureok=True)
# Regression test for a per-channel bit depth libcineon doesn't recognize
# (used to assert/abort inside CineonHeader.cpp instead of erroring out).
command += info_command ("src/broken_bitdepth2.cin", verbose=False, failureok=True)
# Regression test: a 2 KB Cineon header declaring a ~12 GB image
# (46341x46341x3) -- a decompression bomb the compression-ratio guard must
# reject before any large allocation.
command += info_command ("src/bomb-46341.cin", verbose=False, failureok=True)

# Regression test: end-of-line padding. Rows must be located using their
# absolute line number, which only shows up when reading scanlines one at a
# time.
eolnpad_tests = ""
for b in [ 8, 10 ] :
    command += oiiotool("--pattern fill:top=0,0.5,1:bottom=1,0.25,0 13x9 3 "
                        f"-d uint{b} -o eolnpad-ref-{b}bit.dpx")
    eolnpad_tests += f" src/eolnpad-{b}bit.cin eolnpad-ref-{b}bit.dpx"
# Padding too large for the reader's int must be rejected.
command += info_command ("src/bad-eolpad.cin", verbose=False, failureok=True)

outputs = [ "out.txt" ]

if subprocess.call ([pythonbin, "-c", "import OpenImageIO"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL) == 0 :
    redirect_push ("eolnpad.txt")
    command += pythonbin + " src/test_eolnpad.py" + eolnpad_tests + redirect + " ;"
    redirect_pop ()
