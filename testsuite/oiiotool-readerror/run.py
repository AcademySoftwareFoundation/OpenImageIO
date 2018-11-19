#!/usr/bin/env python

# change redirection to send stderr to a separate file
redirect = " >> out.txt 2> out.err.txt "

command += oiiotool ("src/incomplete.exr -o out.exr")
failureok = 1

outputs = [ "out.txt", "out.err.txt" ]
