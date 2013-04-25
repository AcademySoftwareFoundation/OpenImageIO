#!/usr/bin/python 

imagedir = "../texture-field3d"
files = [ "dense_float.f3d", "sparse_float.f3d",
          "dense_half.f3d", "sparse_half.f3d" ]

for f in files:
    command += rw_command (imagedir, f)
