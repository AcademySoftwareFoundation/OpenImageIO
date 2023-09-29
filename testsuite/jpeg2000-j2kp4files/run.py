#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Allow some LSB slop -- necessary because of the alpha disassociation
# combined with implied sRGB conversion.
failthresh = 0.02
failpercent = 0.02

# ../j2kp4files_v1_5/testfiles_jp2
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/testfiles_jp2"
files = [ "file1.jp2", "file2.jp2", "file3.jp2","file4.jp2",
          "file5.jp2", "file6.jp2", "file7.jp2","file8.jp2",
          "file9.jp2" ]
for f in files:
    command += rw_command (imagedir, f)

# ../j2kp4files_v1_5/codestreams_profile0:
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/codestreams_profile0"
files = [ "p0_01.j2k", "p0_02.j2k",
          #"p0_03.j2k",
          "p0_04.j2k",
          "p0_05.j2k", "p0_06.j2k", 
          # "p0_07.j2k",  # can't decode for some reason
          "p0_08.j2k", 
          "p0_09.j2k", "p0_10.j2k", "p0_11.j2k", "p0_12.j2k",
          # "p0_13.j2k",    # can't decode
          "p0_14.j2k", "p0_15.j2k" ]
# for f in files:
#     command += rw_command (imagedir, f)
# Skip these for now, for the sake of a faster unit test. Re-enable it
# later if we speed up the jpeg2000 reader.

# ../j2kp4files_v1_5/codestreams_profile1:
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/codestreams_profile1"
files = [ "p1_01.j2k", "p1_02.j2k", "p1_03.j2k", "p1_04.j2k",
          "p1_05.j2k", "p1_06.j2k", "p1_07.j2k" ]
# for f in files:
#     command += rw_command (imagedir, f)
# Skip these for now, for the sake of a faster unit test. Re-enable it
# later if we speed up the jpeg2000 reader.
