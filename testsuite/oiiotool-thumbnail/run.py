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

# Test that the -i:get_thumbnail=1 read modifier produces the same thumbnail
# as the equivalent --get-thumbnail command above
command += oiiotool ("-i:get_thumbnail=1 " + psd + " -o thumb_i.tif")
command += info_command ("thumb_i.tif", verbose=False, hash=False)
command += oiiotool ("--diff thumb_i.tif thumb.tif")

# Test that autoorient/autocc act on the thumbnail not the main image
command += oiiotool ("--autoorient -i:get_thumbnail=1 " + psd
                     + " --echo \"autoorient {TOP.width}x{TOP.height}\"")
command += oiiotool ("--autocc -i:get_thumbnail=1 " + psd
                     + " --echo \"autocc {TOP.width}x{TOP.height}\"")

# Test that -i:get_thumbnail=1 mirrors `--get-thumbnail`
command += oiiotool ("-i:get_thumbnail=1 " + no_thumb, failureok=True)
command += oiiotool ("-i:get_thumbnail=1:fail=0 " + no_thumb + " --echo \"fail=0 path completed\"")
command += oiiotool ("-i:get_thumbnail=1:index=1 " + psd, failureok=True)

# Test that -i:get_thumbnail=0 returns the main image
command += oiiotool ("-i:get_thumbnail=0 " + psd + " --echo \"get_thumbnail=0 {TOP.width}x{TOP.height}\"")

outputs = [ "thumb.tif", "out.txt" ]
