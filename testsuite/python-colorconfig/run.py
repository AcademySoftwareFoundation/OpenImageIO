#!/usr/bin/env python

from __future__ import absolute_import
import os

os.putenv('OCIO', colorconfig_file)

command += pythonbin + " src/test_colorconfig.py > out.txt"

