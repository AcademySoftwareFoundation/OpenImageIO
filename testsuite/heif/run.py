#!/usr/bin/env python

imagedir = "ref/"
files = [ "IMG_7702_small.heic" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))
