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
# Regression test: pixel data cut short must fail the read, not return
# uninitialized memory (both the 10-bit filled and generic per-row paths).
command += oiiotool ("--testread src/truncated-10bit-filled.cin", failureok=True)
command += oiiotool ("--testread src/truncated-8bit-eolpad.cin", failureok=True)
# Regression test: user data running past the end of the file is an error.
command += info_command ("src/bad-usersize.cin", verbose=False, failureok=True)

outputs = [ "out.txt" ]
