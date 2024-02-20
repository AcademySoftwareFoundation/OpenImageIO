#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1"

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/targa"

files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA",
          "round_grill.tga" ]
for f in files:
    command += rw_command (imagedir, f)


# Test corrupted files
command += iconvert("-v src/crash1.tga crash1.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash2.tga -o crash2.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash3.tga -o crash3.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash4.tga -o crash4.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash5.tga -o crash5.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash6.tga -o crash6.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash1707.tga -o crash1707.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash1708.tga -o crash1708.exr", failureok = True)
command += oiiotool("--oiioattrib limits:imagesize_MB 1024 "
                    "--oiioattrib try_all_readers 0 "
                    "src/crash3952.tga -o crash3952.exr", failureok = True)

# Test odds and ends, unusual files
command += rw_command("src", "1x1.tga")

outputs += [ 'out.txt' ]
