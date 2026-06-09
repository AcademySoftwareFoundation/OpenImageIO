#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Capture error output
redirect = " >> out.txt 2>&1 "

psd = "src/with-thumbnail.psd"
no_thumb = "../common/tahoe-small.tif"

# Test extracting a present thumbnail
command += oiiotool (psd + " --get-thumbnail -o thumb.tif")
command += info_command ("thumb.tif", verbose=False, hash=False)
command += oiiotool (psd + " --get-thumbnail:index=0 -o thumb0.tif")
command += info_command ("thumb0.tif", verbose=False, hash=False)
command += oiiotool (psd + " --get-thumbnail:fail=0 -o thumb_f0.tif")
command += info_command ("thumb_f0.tif", verbose=False, hash=False)
command += oiiotool (psd + " --get-thumbnail:foo=1 -o thumb_foo.tif")
command += info_command ("thumb_foo.tif", verbose=False, hash=False)

# Test stack integrity
command += oiiotool (psd + " --dup --get-thumbnail -o thumb_dup.tif --pop -o full.tif")
command += info_command ("thumb_dup.tif", verbose=False, hash=False)
command += info_command ("full.tif", verbose=False, hash=False)

# Test missing thumbnail
command += oiiotool (no_thumb + " --get-thumbnail", failureok=True)
command += oiiotool (no_thumb + " --get-thumbnail:fail=0 --echo \"fail=0 path completed\"")
command += oiiotool (no_thumb + " --get-thumbnail:fail=0 -o empty_thumb.tif", failureok=True)
command += oiiotool (no_thumb + " --get-thumbnail:fail=0 --if \"{TOP.width}\" -o guarded.tif --else --echo \"no thumbnail, skipped -o\" --endif")

# Test index modifier
command += oiiotool (no_thumb + " --get-thumbnail:index=1", failureok=True)
command += oiiotool (no_thumb + " --get-thumbnail:index=-1", failureok=True)
command += oiiotool (psd + " --get-thumbnail:index=1", failureok=True)
command += oiiotool (no_thumb + " --get-thumbnail:index=1:fail=0", failureok=True)

outputs = [ "thumb.tif", "out.txt" ]
