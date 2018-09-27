#!/usr/bin/env python 

shutil.copyfile ("../common/textures/grid.tx", "grid.tx")

command += pythonbin + " src/test_imageinput.py > out.txt"

