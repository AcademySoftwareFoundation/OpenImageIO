#!/usr/bin/python 

imagedir = parent + "/webp-images/"
files = [ "1.webp", "2.webp", "3.webp", "4.webp" ]
for f in files:
    command = command + info_command (imagedir + f)
