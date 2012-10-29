#!/usr/bin/python

# Format the output from various oiio command line "$tool --help" invocations,
# and munge such that txt2man generates a simple man page with not-too-horrible
# formatting.

from __future__ import print_function
import sys

lines = [l.rstrip().replace('\t', ' '*8) for l in sys.stdin.readlines()]

print('TITLE')
print(lines[0])
print()

print('SYNOPSIS')
for i,line in enumerate(lines[2:]):
    if line.lstrip().startswith('-') or line.lstrip().startswith('Options'):
        optStart = i+2
        break
    print(line)

print('''DESCRIPTION
This program is part of the OpenImageIO (http://www.openimageio.org) tool suite.
Detailed documentation is avaliable in pdf format with the OpenImageIO
distribution.
''')

print('OPTIONS')
for line in lines[optStart:]:
    if not line.startswith(' '):
        print()
        print(line)
    elif not line.lstrip().startswith('-'):
        print(line.lstrip())
    else:
        print(line)
print()
