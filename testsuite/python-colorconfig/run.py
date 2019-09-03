#!/usr/bin/env python

import os

os.putenv('OCIO', colorconfig_file)

command += pythonbin + " src/test_colorconfig.py > out.txt"

