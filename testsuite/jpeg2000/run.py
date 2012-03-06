#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# Start off
command = ""

# ../j2kp4files_v1_5/codestreams_profile0:
# p0_01.j2k to p_0_02.j2k images
imagedir = runtest.parent + "/j2kp4files_v1_5/codestreams_profile0"
command = command + runtest.rw_command (imagedir, "p0_01.j2k")
command = command + runtest.rw_command (imagedir, "p0_02.j2k")
command = command + runtest.rw_command (imagedir, "p0_03.j2k")
command = command + runtest.rw_command (imagedir, "p0_04.j2k")
command = command + runtest.rw_command (imagedir, "p0_05.j2k")
command = command + runtest.rw_command (imagedir, "p0_06.j2k")
command = command + runtest.rw_command (imagedir, "p0_10.j2k")
command = command + runtest.rw_command (imagedir, "p0_11.j2k")
command = command + runtest.rw_command (imagedir, "p0_12.j2k")
command = command + runtest.rw_command (imagedir, "p0_14.j2k")

# Outputs to check against references
outputs = [ "out.txt" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
