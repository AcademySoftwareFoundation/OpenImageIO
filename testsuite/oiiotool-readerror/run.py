#!/usr/bin/env python

command += oiiotool ("src/incomplete.exr -o out.exr 2> out.err.txt")
failureok = 1

outputs = [ "out.txt", "out.err.txt" ]
