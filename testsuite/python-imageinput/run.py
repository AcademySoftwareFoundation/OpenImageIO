#!/usr/bin/env python 

shutil.copyfile ("../common/textures/grid.tx", "grid.tx")

command += "python src/test_imageinput.py > out.txt"

