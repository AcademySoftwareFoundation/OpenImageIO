#!/usr/bin/env python

# save the error output
redirect = " >> out.txt 2>&1 "

# This file has a corrupted header. Reported in #1759
command += info_command ("src/corrupt-header.jpg", safematch=True)

# Checking the error output is important for this test
#outputs = [ "out.txt", "out.err.txt" ]
failureok = 1
