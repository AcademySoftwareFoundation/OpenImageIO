#!/usr/bin/python 

# ../j2kp4files_v1_5/codestreams_profile0:
imagedir = parent + "/j2kp4files_v1_5/codestreams_profile0"
files = [ "p0_01.j2k", "p0_02.j2k", "p0_03.j2k", "p0_04.j2k",
          "p0_05.j2k", "p0_06.j2k", "p0_10.j2k", "p0_11.j2k",
          "p0_12.j2k", "p0_14.j2k" ]
for f in files:
    command += rw_command (imagedir, f)
