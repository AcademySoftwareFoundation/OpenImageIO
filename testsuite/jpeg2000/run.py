#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = [".."] + sys.path
import runtest

# Start off
hi = "echo hi"
command = hi + "> out.txt"

# ../j2kp4files_v1_5/codestreams_profile0:
# p0_01.j2k to p_0_02.j2k images
imagedir = "../../../j2kp4files_v1_5/codestreams_profile0"
command = command + "; " + runtest.rw_command (imagedir, "p0_01.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_02.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_03.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_04.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_05.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_06.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_10.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_11.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_12.j2k", path)
command = command + "; " + runtest.rw_command (imagedir, "p0_14.j2k", path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = ["p0_01.j2k", "p0_02.j2k", "p0_03.j2k", "p0_04.j2k", "p0_05.j2k",
              "p0_06.j2k", "p0_09.j2k", "p0_10.j2k", "p0_11.j2k", "p0_12.j2k",
              "p0_14.j2k"]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
