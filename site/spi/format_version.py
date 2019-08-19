#!/usr/bin/env python

# This helper script takes a major.minor.patch semantic version string
# ("2.1.1") and reformats it into the dot-less zero-padded version like
# "20101".  This is necessary to turn external release designations into
# integer SpComp2 version numbers.

from __future__ import print_function
import sys

if len(sys.argv) != 2 :
    print('Need 1 argument: version number with dots')
    exit(1)

vers = sys.argv[1]
parts = vers.split('.')
print ('{:d}{:02d}{:02d}'.format(int(parts[0]), int(parts[1]), int(parts[2])))
